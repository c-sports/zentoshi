// Copyright (c) 2019-2020 Zentoshi LLC
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <coldgains.h>

#include <chainparams.h>
#include <consensus/consensus.h>
#include <consensus/validation.h>
#include <interfaces/wallet.h>
#include <pubkey.h>
#include <script/signingprovider.h>
#include <script/standard.cpp>
#include <util/validation.h>
#include <util/system.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/wallet.h>

#include <vector>

#include <boost/variant.hpp>
#include <boost/variant/apply_visitor.hpp>

unsigned int GetCandidateInputAge(uint256 txHash)
{
    const unsigned int currentHeight = ::ChainActive().Height();
    uint256 hashBlock;
    CTransactionRef candidateInput;
    if (!GetTransaction(txHash, candidateInput, Params().GetConsensus(), hashBlock))
        return 0;
    if (hashBlock == uint256())
        return 0;
    unsigned int deltaHeight = currentHeight - ::LookupBlockIndex(hashBlock)->nHeight;
    return deltaHeight;
}

CScript createColdGainsScript(CScript& gainsAddress, const int blockHeight)
{
    const int nHeight = ::ChainActive().Height() + blockHeight;
    return CScript() << CScriptNum(nHeight) << OP_CHECKLOCKTIMEVERIFY << OP_DROP << ToByteVector(gainsAddress) << OP_CHECKSIG;
}

CAmount coldGainReturn(const CAmount origValue)
{
    return (origValue * 1.1);
}

bool createColdGainTransaction(COutput& out, CScript& gainsAddress, const int64_t blockHeight, const CAmount origValue)
{
    auto pwallet = GetMainWallet();

    //! build the transaction
    CMutableTransaction gainsTransaction;
    gainsTransaction.nVersion = 8;
    gainsTransaction.nType = TRANSACTION_COLDGAINS;
    gainsTransaction.vin.resize(1);
    gainsTransaction.vin.clear();
    gainsTransaction.vin.emplace_back(out.tx->tx->vin[out.i].prevout);
    gainsTransaction.vout.resize(1);
    gainsTransaction.vout[0].scriptPubKey = createColdGainsScript(gainsAddress, blockHeight);
    gainsTransaction.vout[0].nValue = origValue;

    int nIn = 0;
    SignatureData sigdata;
    SigningProvider keystore;
    ProduceSignature(keystore, MutableTransactionSignatureCreator(&gainsTransaction, nIn, gainsTransaction.vout[0].nValue, SIGHASH_ALL), gainsTransaction.vout[0].scriptPubKey, sigdata);

    CTransactionRef tx = MakeTransactionRef(gainsTransaction);
    mapValue_t mapValue;
    CValidationState state;
    if (!pwallet->CommitTransaction(tx, std::move(mapValue), {}, state))
        return false;

    if (!checkColdGainTransaction(gainsTransaction, Params().GetConsensus()))
        LogPrintf("* failed internal tests\n");
    else
        LogPrintf("* passed internal tests\n");
        
    return true;
}

bool checkColdGainTransaction(const CTransaction& tx, const Consensus::Params& consensusParams)
{
    std::vector<unsigned int> coldgainsParams = consensusParams.nColdGainParams;

    //! basic contextual checks
    if (tx.nType != TRANSACTION_COLDGAINS || tx.nVersion != 0)
        return false;

    //! one vin, one vout only
    unsigned int n;
    n = tx.vin.size();
    if (n != 1) return false;
    n = tx.vout.size();
    if (n != 1) return false;

    //! fetch input tx
    uint256 sourceBlock;
    CTransactionRef sourceTransaction;
    const COutPoint txin = tx.vin[0].prevout;
    if (!GetTransaction(txin.hash, sourceTransaction, Params().GetConsensus(), sourceBlock))
        return false;

    //! scriptPubKey not allowed to change
    const auto src = sourceTransaction->vout[txin.n].scriptPubKey;
    const auto dest = tx.vout[0].scriptPubKey;
    if (src != dest) return false;

    //! age limits
    const int minagelimit = coldgainsParams.at(ColdGains::MINAGE);
    const int maxagelimit = coldgainsParams.at(ColdGains::MAXAGE);
    const int inputAge = GetCandidateInputAge(txin.hash);
    if (inputAge < minagelimit || inputAge > maxagelimit)
        return false;

    //! amount limits (input)
    const CAmount minamtlimit = coldgainsParams.at(ColdGains::MINAMOUNT);
    const CAmount maxamtlimit = coldgainsParams.at(ColdGains::MAXAMOUNT);
    const auto inpValue = sourceTransaction->vout[txin.n].nValue;
    if (inpValue < minamtlimit || inpValue > maxamtlimit)
        return false;

    //! amount limits (output)
    const auto outValue = tx.GetValueOut();
    const auto expectedGain = coldGainReturn(inpValue);
    if (outValue != expectedGain)
        return false;

    return true;
}
