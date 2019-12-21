// Copyright (c) 2012-2013 The PPCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp>
#include <boost/lexical_cast.hpp>

#include <chainparams.h>
#include <index/txindex.cpp> // ew.. iknowright? (baz)
#include <init.h>
#include <kernel.h>
#include <script/interpreter.h>
#include <timedata.h>
#include <txdb.h>
#include <policy/policy.h>
#include <util/system.h>
#include <util/time.h>
#include <wallet/db.h>
#include <wallet/wallet.h>
#include <validation.h>

#include <numeric>

#define PRI64x  "llx"

using namespace std;

// Modifier interval: time to elapse before new modifier is computed
// Set to 3-hour for production network and 20-minute for test network
unsigned int nModifierInterval = MODIFIER_INTERVAL;
unsigned int getIntervalVersion(bool fTestNet)
{
    if (fTestNet)
        return MODIFIER_INTERVAL_TESTNET;
    else
        return MODIFIER_INTERVAL;
}

// Hard checkpoints of stake modifiers to ensure they are deterministic
static std::map<int, unsigned int> mapStakeModifierCheckpoints = {};

// Get time weight
int64_t GetWeight(int64_t nIntervalBeginning, int64_t nIntervalEnd)
{
    return nIntervalEnd - nIntervalBeginning - Params().GetConsensus().nStakeMinAge;
}

// Get the last stake modifier and its generation time from a given block
static bool GetLastStakeModifier(const CBlockIndex* pindex, uint64_t& nStakeModifier, int64_t& nModifierTime)
{
    if (!pindex)
        return error("GetLastStakeModifier: null pindex");
    while (pindex && pindex->pprev && !pindex->GeneratedStakeModifier())
        pindex = pindex->pprev;
    if (!pindex->GeneratedStakeModifier())
        return error("GetLastStakeModifier: no generation at genesis block");
    nStakeModifier = pindex->nStakeModifier;
    nModifierTime = pindex->GetBlockTime();
    return true;
}

// Get selection interval section (in seconds)
static int64_t GetStakeModifierSelectionIntervalSection(int nSection)
{
    assert (nSection >= 0 && nSection < 64);
    return (Params().GetConsensus().nModifierInterval * 63 / (63 + ((63 - nSection) * (MODIFIER_INTERVAL_RATIO - 1))));
}

// Get stake modifier selection interval (in seconds)
static int64_t GetStakeModifierSelectionInterval()
{
    int64_t nSelectionInterval = 0;
    for (int nSection = 0; nSection < 64; nSection++)
        nSelectionInterval += GetStakeModifierSelectionIntervalSection(nSection);
    return nSelectionInterval;
}

// select a block from the candidate blocks in vSortedByTimestamp, excluding
// already selected blocks in vSelectedBlocks, and with timestamp up to
// nSelectionIntervalStop.
static bool SelectBlockFromCandidates(
        vector<pair<int64_t, uint256> >& vSortedByTimestamp,
        map<uint256, const CBlockIndex*>& mapSelectedBlocks,
        int64_t nSelectionIntervalStop, uint64_t nStakeModifierPrev,
        const CBlockIndex** pindexSelected)
{
    bool fSelected = false;
    arith_uint256 hashBest = 0;
    *pindexSelected = nullptr;
    for(const auto &item : vSortedByTimestamp)
    {
        if (!::BlockIndex().count(item.second))
            return error("SelectBlockFromCandidates: failed to find block index for candidate block %s", item.second.ToString().c_str());
        const CBlockIndex* pindex = ::BlockIndex()[item.second];
        if (fSelected && pindex->GetBlockTime() > nSelectionIntervalStop)
            break;
        if (mapSelectedBlocks.count(pindex->GetBlockHash()) > 0)
            continue;
        // compute the selection hash by hashing its proof-hash and the
        // previous proof-of-stake modifier
        uint256 hashProof = pindex->IsProofOfStake()? pindex->hashProofOfStake : pindex->GetBlockHash();
        CDataStream ss(SER_GETHASH, 0);
        ss << hashProof << nStakeModifierPrev;
        arith_uint256 hashSelection = UintToArith256(Hash(ss.begin(), ss.end()));
        // the selection hash is divided by 2**32 so that proof-of-stake block
        // is always favored over proof-of-work block. this is to preserve
        // the energy efficiency property
        if (pindex->IsProofOfStake())
            hashSelection >>= 32;
        if (fSelected && hashSelection < hashBest)
        {
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*) pindex;
        }
        else if (!fSelected)
        {
            fSelected = true;
            hashBest = hashSelection;
            *pindexSelected = (const CBlockIndex*) pindex;
        }
    }
    LogPrint(BCLog::KERNEL, "%s : selection hash=%s\n", __func__, hashBest.ToString().c_str());
    return fSelected;
}

// Stake Modifier (hash modifier of proof-of-stake):
// The purpose of stake modifier is to prevent a txout (coin) owner from
// computing future proof-of-stake generated by this txout at the time
// of transaction confirmation. To meet kernel protocol, the txout
// must hash with a future stake modifier to generate the proof.
// Stake modifier consists of bits each of which is contributed from a
// selected block of a given block group in the past.
// The selection of a block is based on a hash of the block's proof-hash and
// the previous stake modifier.
// Stake modifier is recomputed at a fixed time interval instead of every
// block. This is to make it difficult for an attacker to gain control of
// additional bits in the stake modifier, even after generating a chain of
// blocks.
bool ComputeNextStakeModifier(const CBlockIndex* pindexCurrent, uint64_t &nStakeModifier, bool& fGeneratedStakeModifier)
{
    const Consensus::Params& params = Params().GetConsensus();
    const CBlockIndex* pindexPrev = pindexCurrent->pprev;
    nStakeModifier = 0;
    fGeneratedStakeModifier = false;
    if (!pindexPrev)
    {
        fGeneratedStakeModifier = true;
        return true;  // genesis block's modifier is 0
    }
    // First find current stake modifier and its generation block time
    // if it's not old enough, return the same stake modifier
    int64_t nModifierTime = 0;
    if (!GetLastStakeModifier(pindexPrev, nStakeModifier, nModifierTime))
        return error("%s: unable to get last modifier", __func__);

    LogPrint(BCLog::KERNEL, "%s: prev modifier=0x%016x time=%s epoch=%u\n", __func__, nStakeModifier, FormatISO8601DateTime(nModifierTime), (unsigned int)nModifierTime);
    if (nModifierTime / params.nModifierInterval >= pindexPrev->GetBlockTime() / params.nModifierInterval)
    {
        LogPrint(BCLog::KERNEL, "%s: no new interval keep current modifier: pindexPrev nHeight=%d nTime=%u\n",
            __func__, pindexPrev->nHeight, (unsigned int)pindexPrev->GetBlockTime());
        return true;
    }
    if (nModifierTime / params.nModifierInterval >= pindexCurrent->GetBlockTime() / params.nModifierInterval)
    {
        LogPrint(BCLog::KERNEL, "%s: no new interval keep current modifier: pindexCurrent nHeight=%d nTime=%u\n",
            __func__, pindexCurrent->nHeight, (unsigned int)pindexCurrent->GetBlockTime());
        return true;
    }

    // Sort candidate blocks by timestamp
    std::vector<std::pair<int64_t, uint256> > vSortedByTimestamp;
    vSortedByTimestamp.reserve(64 * params.nModifierInterval / params.nPosTargetTimespan);
    int64_t nSelectionInterval = GetStakeModifierSelectionInterval();
    int64_t nSelectionIntervalStart = (pindexPrev->GetBlockTime() / params.nModifierInterval) * params.nModifierInterval - nSelectionInterval;
    const CBlockIndex* pindex = pindexPrev;
    while (pindex && pindex->GetBlockTime() >= nSelectionIntervalStart)
    {
        vSortedByTimestamp.push_back(make_pair(pindex->GetBlockTime(), pindex->GetBlockHash()));
        pindex = pindex->pprev;
    }
    int nHeightFirstCandidate = pindex ? (pindex->nHeight + 1) : 0;
    reverse(vSortedByTimestamp.begin(), vSortedByTimestamp.end());
    sort(vSortedByTimestamp.begin(), vSortedByTimestamp.end());

    // Select 64 blocks from candidate blocks to generate stake modifier
    uint64_t nStakeModifierNew = 0;
    int64_t nSelectionIntervalStop = nSelectionIntervalStart;
    map<uint256, const CBlockIndex*> mapSelectedBlocks;
    for (int nRound=0; nRound<min(64, (int)vSortedByTimestamp.size()); nRound++)
    {
        // add an interval section to the current selection round
        nSelectionIntervalStop += GetStakeModifierSelectionIntervalSection(nRound);
        // select a block from the candidates of current round
        if (!SelectBlockFromCandidates(vSortedByTimestamp, mapSelectedBlocks, nSelectionIntervalStop, nStakeModifier, &pindex))
            return error("ComputeNextStakeModifier: unable to select block at round %d", nRound);
        // write the entropy bit of the selected block
        nStakeModifierNew |= (((uint64_t)pindex->GetStakeEntropyBit()) << nRound);
        // add the selected block from candidates to selected list
        mapSelectedBlocks.insert(make_pair(pindex->GetBlockHash(), pindex));
        LogPrint(BCLog::KERNEL, "%s : selected round %d stop=%s height=%d bit=%d\n", __func__, nRound, DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nSelectionIntervalStop).c_str(), pindex->nHeight, pindex->GetStakeEntropyBit());
    }

    // Print selection map for visualization of the selected blocks
    {
        string strSelectionMap = "";
        // '-' indicates proof-of-work blocks not selected
        strSelectionMap.insert(0, pindexPrev->nHeight - nHeightFirstCandidate + 1, '-');
        pindex = pindexPrev;
        while (pindex && pindex->nHeight >= nHeightFirstCandidate)
        {
            // '=' indicates proof-of-stake blocks not selected
            if (pindex->IsProofOfStake())
                strSelectionMap.replace(pindex->nHeight - nHeightFirstCandidate, 1, "=");
            pindex = pindex->pprev;
        }
        for (const auto& item : mapSelectedBlocks)
        {
            // 'S' indicates selected proof-of-stake blocks
            // 'W' indicates selected proof-of-work blocks
            strSelectionMap.replace(item.second->nHeight - nHeightFirstCandidate, 1, item.second->IsProofOfStake()? "S" : "W");
        }
        LogPrint(BCLog::KERNEL, "ComputeNextStakeModifier: selection height [%d, %d] map %s\n", nHeightFirstCandidate, pindexPrev->nHeight, strSelectionMap);
    }
    LogPrint(BCLog::KERNEL, "ComputeNextStakeModifier: new modifier=0x%016x time=%s\n", nStakeModifierNew, DateTimeStrFormat("%Y-%m-%d %H:%M:%S", pindexPrev->GetBlockTime()).c_str());

    nStakeModifier = nStakeModifierNew;
    fGeneratedStakeModifier = true;
    return true;
}

static bool GetKernelStakeModifier(uint256 hashBlockFrom, unsigned int nTimeTx, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake)
{
    nStakeModifier = 0;
    if (!::BlockIndex().count(hashBlockFrom))
        return error("GetKernelStakeModifier() : block not indexed");

    const CBlockIndex* pindexFrom = ::BlockIndex()[hashBlockFrom];
    nStakeModifierHeight = pindexFrom->nHeight;
    nStakeModifierTime = pindexFrom->GetBlockTime();
    int64_t nStakeModifierSelectionInterval = GetStakeModifierSelectionInterval();
    const CBlockIndex* pindex = pindexFrom;
    CBlockIndex* pindexNext = ::ChainActive()[pindexFrom->nHeight + 1];

    // loop to find the stake modifier later by a selection interval
    while (nStakeModifierTime < pindexFrom->GetBlockTime() + nStakeModifierSelectionInterval) {
        if (!pindexNext) {
            nStakeModifierHeight = pindexFrom->nHeight;
            nStakeModifierTime = pindexFrom->GetBlockTime();
            if(pindex->GeneratedStakeModifier())
               nStakeModifier = pindex->nStakeModifier;
            return true;
        }
        pindex = pindexNext;
        pindexNext = ::ChainActive()[pindexNext->nHeight + 1];
        if (pindex->GeneratedStakeModifier()) {
            nStakeModifierHeight = pindex->nHeight;
            nStakeModifierTime = pindex->GetBlockTime();
        }
    }
    nStakeModifier = pindex->nStakeModifier;
    return true;
}

bool CheckStakeKernelHash(unsigned int nBits, const CBlockHeader& blockFrom, const CTransactionRef& txPrev, const COutPoint& prevout, unsigned int nTimeTx, uint256& hashProofOfStake, bool fMinting, bool fValidate)
{
    auto txPrevTime = blockFrom.nTime;
    if (nTimeTx < txPrevTime)  // Transaction timestamp violation
        return error("CheckStakeKernelHash() : nTime violation");

    auto nStakeMinAge = Params().GetConsensus().nStakeMinAge;
    auto nStakeMaxAge = Params().GetConsensus().nStakeMaxAge;
    unsigned int nTimeBlockFrom = blockFrom.GetBlockTime();
    if (nTimeBlockFrom + nStakeMinAge > nTimeTx) // Min age requirement
        return error("CheckStakeKernelHash() : min age violation");

    arith_uint256 bnTargetPerCoinDay;
    bnTargetPerCoinDay.SetCompact(nBits);
    CAmount nValueIn = txPrev->vout[prevout.n].nValue;
    // v0.3 protocol kernel hash weight starts from 0 at the 30-day min age
    // this change increases active coins participating the hash and helps
    // to secure the network when proof-of-stake difficulty is low
    int64_t nTimeWeight = std::min<int64_t>(nTimeTx - txPrevTime, nStakeMaxAge - nStakeMinAge);
    arith_uint256 bnCoinDayWeight = nValueIn * nTimeWeight / COIN / 200;

    // Calculate hash
    CDataStream ss(SER_GETHASH, 0);
    uint64_t nStakeModifier = 0;
    int nStakeModifierHeight = 0;
    int64_t nStakeModifierTime = 0;

    if (!GetKernelStakeModifier(blockFrom.GetHash(), nTimeTx, nStakeModifier, nStakeModifierHeight, nStakeModifierTime, false))
        return false;
    ss << nStakeModifier;
    ss << nTimeBlockFrom << txPrevTime << prevout.n << nTimeTx;
    hashProofOfStake = Hash(ss.begin(), ss.end());

    // Debugging stake kernel
    if (gArgs.GetBoolArg("-debug", true)) {
       bool fStakeValid = (UintToArith256(hashProofOfStake) > bnCoinDayWeight * bnTargetPerCoinDay);
       LogPrint(BCLog::KERNEL, "hashProofOfStake %s (blockcandidate: %s)\n", hashProofOfStake.ToString().c_str(), !fStakeValid ? "Y" : "N");
    }

    // Now check if proof-of-stake hash meets target protocol
    if (UintToArith256(hashProofOfStake) > bnCoinDayWeight * bnTargetPerCoinDay)
        return false;

    return true;
}

bool CheckKernelScript(CScript scriptVin, CScript scriptVout)
{
    auto extractKeyID = [](CScript scriptPubKey) {

        int resultType = 0;
        std::vector<std::vector<unsigned char>> vSolutions;
        txnouttype whichType = Solver(scriptPubKey, vSolutions);

        CKeyID keyID;
        {
            if (whichType == TX_PUBKEYHASH)
            {
                resultType = 1;
                keyID = CKeyID(uint160(vSolutions[0]));
            }
            else if(whichType == TX_PUBKEY)
            {
                resultType = 2;
                keyID = CPubKey(vSolutions[0]).GetID();
            }
            else if(whichType == TX_WITNESS_V0_SCRIPTHASH ||
                    whichType == TX_WITNESS_V0_KEYHASH)
            {
                resultType = 3;
                keyID = CKeyID(uint160(vSolutions[0]));
            }
        }
        LogPrint(BCLog::KERNEL, "CheckKernelScript()::Type %d\n", resultType);

        return keyID;
    };

    return extractKeyID(scriptVin) == extractKeyID(scriptVout);
}

// Check kernel hash target and coinstake signature
bool CheckProofOfStake(const CBlock &block, uint256& hashProofOfStake, const CBlockIndex* pindexPrev)
{
    const CTransactionRef &tx = block.vtx[1];
    if (!tx->IsCoinStake())
        return error("CheckProofOfStake() : called on non-coinstake %s", tx->GetHash().ToString().c_str());

    // Kernel (input 0) must match the stake hash target per coin age (nBits)
    const CTxIn& txin = tx->vin[0];

    // First try finding the previous transaction in database
    uint256 hashBlock;
    CTransactionRef txPrev;
    const auto &cons = Params().GetConsensus();

    if (!GetTransaction(txin.prevout.hash, txPrev, cons, hashBlock))
        return error("CheckProofOfStake() : INFO: read txPrev failed");

    CTxOut prevTxOut = txPrev->vout[txin.prevout.n];

    // Find block index
    CBlockIndex* pindex = nullptr;
    BlockMap::iterator it = ::BlockIndex().find(hashBlock);
    if (it != ::BlockIndex().end())
        pindex = it->second;
    else
        return error("CheckProofOfStake() : read block failed");

    // Read block header
    CBlock blockprev;
    if (!ReadBlockFromDisk(blockprev, pindex->GetBlockPos(), cons))
        return error("CheckProofOfStake(): INFO: failed to find block");

    if(!CheckKernelScript(prevTxOut.scriptPubKey, tx->vout[1].scriptPubKey))
        return error("CheckProofOfStake() : INFO: check kernel script failed on coinstake %s, hashProof=%s \n", tx->GetHash().ToString().c_str(), hashProofOfStake.ToString().c_str());

    if (!CheckStakeKernelHash(block.nBits, blockprev, txPrev, txin.prevout, block.nTime, hashProofOfStake, false, true))
        return error("CheckProofOfStake() : INFO: check kernel failed on coinstake %s, hashProof=%s \n", tx->GetHash().ToString().c_str(), hashProofOfStake.ToString().c_str());

    return true;
}

// Get stake modifier checksum
unsigned int GetStakeModifierChecksum(const CBlockIndex* pindex)
{
    assert (pindex->pprev || pindex->GetBlockHash() == Params().GetConsensus().hashGenesisBlock);
    // Hash previous checksum with flags, hashProofOfStake and nStakeModifier
    CDataStream ss(SER_GETHASH, 0);
    if (pindex->pprev)
        ss << pindex->pprev->nStakeModifierChecksum;
    ss << pindex->nFlags << pindex->hashProofOfStake << pindex->nStakeModifier;
    arith_uint256 hashChecksum = UintToArith256(Hash(ss.begin(), ss.end()));
    hashChecksum >>= (256 - 32);
    return hashChecksum.GetLow64();
}

// Check stake modifier hard checkpoints
bool CheckStakeModifierCheckpoints(int nHeight, unsigned int nStakeModifierChecksum)
{
    if (mapStakeModifierCheckpoints.count(nHeight))
        return nStakeModifierChecksum == mapStakeModifierCheckpoints[nHeight];
    return true;
}
