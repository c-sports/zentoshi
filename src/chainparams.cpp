// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2014-2019 The Dash Core developers
// Copyright (c) 2019-2020 Zentoshi LLC
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>

#include <arith_uint256.h>
#include <chainparamsseeds.h>
#include <consensus/merkle.h>
#include <tinyformat.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <versionbitsinfo.h>

#include <assert.h>

#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

#define FROMGENESIS 1

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "zentoshi - kaizen inspired, community driven";
    const CScript genesisOutputScript = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

void CChainParams::UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout, int64_t nWindowSize, int64_t nThreshold)
{
    consensus.vDeployments[d].nStartTime = nStartTime;
    consensus.vDeployments[d].nTimeout = nTimeout;

    if(nWindowSize != -1) {
        consensus.vDeployments[d].nWindowSize = nWindowSize;
    }

    if(nThreshold != -1) {
        consensus.vDeployments[d].nThreshold = nThreshold;
    }
}

void CChainParams::UpdateDIP3Parameters(int nActivationHeight, int nEnforcementHeight)
{
    consensus.DIP0003Height = nActivationHeight;
    consensus.DIP0003EnforcementHeight = nEnforcementHeight;
}

void CChainParams::UpdateBudgetParameters(int nMasternodePaymentsStartBlock, int nBudgetPaymentsStartBlock, int nSuperblockStartBlock)
{
    consensus.nMasternodePaymentsStartBlock = nMasternodePaymentsStartBlock;
    consensus.nBudgetPaymentsStartBlock = nBudgetPaymentsStartBlock;
    consensus.nSuperblockStartBlock = nSuperblockStartBlock;
}

void CChainParams::UpdateLLMQChainLocks(Consensus::LLMQType llmqType)
{
    consensus.llmqTypeChainLocks = llmqType;
}

// this one is for testing only
static Consensus::LLMQParams llmq5_60 = {
    .type = Consensus::LLMQ_5_60,
    .name = "llmq_5_60",
    .size = 5,
    .minSize = 3,
    .threshold = 3,
    .dkgInterval = 24, // one DKG per hour
    .dkgPhaseBlocks = 2,
    .dkgMiningWindowStart = 10, // dkgPhaseBlocks * 5 = after finalization
    .dkgMiningWindowEnd = 18,
    .dkgBadVotesThreshold = 8,
    .signingActiveQuorumCount = 2, // just a few ones to allow easier testing
    .keepOldConnections = 3,
};

static Consensus::LLMQParams llmq50_60 = {
    .type = Consensus::LLMQ_50_60,
    .name = "llmq_50_60",
    .size = 50,
    .minSize = 40,
    .threshold = 30,
    .dkgInterval = 24, // one DKG per hour
    .dkgPhaseBlocks = 2,
    .dkgMiningWindowStart = 10, // dkgPhaseBlocks * 5 = after finalization
    .dkgMiningWindowEnd = 18,
    .dkgBadVotesThreshold = 40,
    .signingActiveQuorumCount = 24, // a full day worth of LLMQs
    .keepOldConnections = 25,
};

static Consensus::LLMQParams llmq400_60 = {
    .type = Consensus::LLMQ_400_60,
    .name = "llmq_400_60",
    .size = 400,
    .minSize = 300,
    .threshold = 240,
    .dkgInterval = 24 * 12, // one DKG every 12 hours
    .dkgPhaseBlocks = 4,
    .dkgMiningWindowStart = 20, // dkgPhaseBlocks * 5 = after finalization
    .dkgMiningWindowEnd = 28,
    .dkgBadVotesThreshold = 300,
    .signingActiveQuorumCount = 4, // two days worth of LLMQs
    .keepOldConnections = 5,
};

// Used for deployment and min-proto-version signalling, so it needs a higher threshold
static Consensus::LLMQParams llmq400_85 = {
    .type = Consensus::LLMQ_400_85,
    .name = "llmq_400_85",
    .size = 400,
    .minSize = 350,
    .threshold = 340,
    .dkgInterval = 24 * 24, // one DKG every 24 hours
    .dkgPhaseBlocks = 4,
    .dkgMiningWindowStart = 20, // dkgPhaseBlocks * 5 = after finalization
    .dkgMiningWindowEnd = 48, // give it a larger mining window to make sure it is mined
    .dkgBadVotesThreshold = 300,
    .signingActiveQuorumCount = 4, // two days worth of LLMQs
    .keepOldConnections = 5,
};


/**
 * Main network
 */
class CMainParams : public CChainParams
{
public:
    CMainParams() {
        strNetworkID = "main";

        consensus.nFirstPoSBlock = 50;
        consensus.nMasternodePaymentsStartBlock = consensus.nFirstPoSBlock;
        consensus.nMasternodePaymentsIncreaseBlock = consensus.nFirstPoSBlock;
        consensus.nMasternodePaymentsIncreasePeriod = 576*30;
        consensus.nInstantSendConfirmationsRequired = 6;
        consensus.nInstantSendKeepLock = 24;
        consensus.nInstantSendSigsRequired = 6;
        consensus.nInstantSendSigsTotal = 10;
        consensus.nBudgetPaymentsStartBlock = consensus.nFirstPoSBlock * 2;
        consensus.nBudgetPaymentsCycleBlocks = 16616;
        consensus.nBudgetPaymentsWindowBlocks = 100;
        consensus.nSuperblockStartBlock = 500;
        consensus.nSuperblockStartHash = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
        consensus.nSuperblockCycle = 16616;
        consensus.nGovernanceMinQuorum = 10;
        consensus.nGovernanceFilterElements = 20000;
        consensus.nMasternodeCollateral = 5000 * COIN;
        consensus.nMasternodeMinimumConfirmations = 15;
        consensus.BIP16Exception = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
        consensus.BIP34Height = FROMGENESIS;
        consensus.BIP34Hash = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
        consensus.BIP65Height = consensus.nFirstPoSBlock;
        consensus.BIP66Height = consensus.nFirstPoSBlock;
        consensus.CSVHeight = FROMGENESIS;
        consensus.SegwitHeight = FROMGENESIS;
        consensus.DIP0001Height = FROMGENESIS;
        consensus.DIP0003Height = consensus.nFirstPoSBlock;
        consensus.DIP0003EnforcementHeight = 1048576;
        consensus.DIP0003EnforcementHash = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
        consensus.DIP0008Height = consensus.nFirstPoSBlock * 4;
        consensus.powLimit = uint256S("0000ffff00000000000000000000000000000000000000000000000000000000");
        consensus.posLimit = uint256S("0007ffff00000000000000000000000000000000000000000000000000000000");
        consensus.nPowTargetTimespan = 300;
        consensus.nPowTargetSpacing = 60;
        consensus.nPosTargetTimespan = consensus.nPowTargetTimespan / 2;
        consensus.nPosTargetSpacing = consensus.nPowTargetSpacing;
        consensus.nStakeMinAge = 60 * 60;
        consensus.nStakeMaxAge = 60 * 60 * 24 * 30;
        consensus.nMinStakeAmount = 0.5 * COIN;
        consensus.nMinStakeHistory = 60;
        consensus.nModifierInterval = 60;
        consensus.nCoinbaseMaturity = 20;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999;   // December 31, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nWindowSize = 100;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nThreshold = 50;

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1572790000; // Nov 1st, 2019
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1604180000;   // Nov 1st, 2020
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nWindowSize = 100;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nThreshold = 50;

        // Deployment of DIP0001
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nWindowSize = 100;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nThreshold = 50;

        // Deployment of DIP0003
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].bit = 3;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nWindowSize = 100;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nThreshold = 50;

        // Deployment of DIP0008
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].bit = 4;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nWindowSize = 100;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nThreshold = 10;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0000000000000000000000000000000000000000000000000000000000000000");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0000000000000000000000000000000000000000000000000000000000000000");

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0x0f;
        pchMessageStart[1] = 0x0f;
        pchMessageStart[2] = 0x0f;
        pchMessageStart[3] = 0x0f;
        nDefaultPort = 49152;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 1;
        m_assumed_chain_state_size = 0.5;
        nMaxReorganizationDepth = 100;

        // genesis
        uint32_t nTime = 1576801004;
        uint32_t nNonce = 593;
        genesis = CreateGenesisBlock(nTime, nNonce, 0x1f7fff00, 1, 0 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,80);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,142);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,208);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};
        bech32_hrp = "zenx";
        nExtCoinType = 5;

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        // long living quorum params
        consensus.llmqs[Consensus::LLMQ_5_60] = llmq5_60;
        consensus.llmqs[Consensus::LLMQ_50_60] = llmq50_60;
        consensus.llmqs[Consensus::LLMQ_400_60] = llmq400_60;
        consensus.llmqs[Consensus::LLMQ_400_85] = llmq400_85;
        consensus.llmqTypeChainLocks = Consensus::LLMQ_5_60;
        consensus.llmqTypeInstantSend = Consensus::LLMQ_5_60;

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fRequireRoutableExternalIP = true;
        fMineBlocksOnDemand = false;
        fAllowMultipleAddressesFromGroup = true;
        fAllowMultiplePorts = true;

        nPoolMinParticipants = 3;
        nPoolMaxParticipants = 5;
        nFulfilledRequestExpireTime = 60*60;
        vSporkAddresses = {"ZXoR9MgySWXAkoWvso96pcog5J7VXiGPHm"};
        nMinSporkKeys = 1;
        fBIP9CheckMasternodesUpgraded = true;
        consensus.fLLMQAllowDummyCommitments = false;
        m_is_test_chain = false;

        checkpointData = {{{ 0, consensus.hashGenesisBlock }}};
        chainTxData = ChainTxData {};
    }
};

/**
 * Testnet
 */
class CTestNetParams : public CChainParams
{
public:
    CTestNetParams() {
        strNetworkID = "test";

        consensus.nFirstPoSBlock = 50;
        consensus.nMasternodePaymentsStartBlock = consensus.nFirstPoSBlock;
        consensus.nMasternodePaymentsIncreaseBlock = consensus.nFirstPoSBlock;
        consensus.nMasternodePaymentsIncreasePeriod = 576*30;
        consensus.nInstantSendConfirmationsRequired = 6;
        consensus.nInstantSendKeepLock = 24;
        consensus.nInstantSendSigsRequired = 6;
        consensus.nInstantSendSigsTotal = 10;
        consensus.nBudgetPaymentsStartBlock = consensus.nFirstPoSBlock * 2;
        consensus.nBudgetPaymentsCycleBlocks = 16616;
        consensus.nBudgetPaymentsWindowBlocks = 100;
        consensus.nSuperblockStartBlock = 500;
        consensus.nSuperblockStartHash = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
        consensus.nSuperblockCycle = 16616;
        consensus.nGovernanceMinQuorum = 10;
        consensus.nGovernanceFilterElements = 20000;
        consensus.nMasternodeCollateral = 5000 * COIN;
        consensus.nMasternodeMinimumConfirmations = 15;
        consensus.BIP16Exception = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
        consensus.BIP34Height = FROMGENESIS;
        consensus.BIP34Hash = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
        consensus.BIP65Height = consensus.nFirstPoSBlock;
        consensus.BIP66Height = consensus.nFirstPoSBlock;
        consensus.CSVHeight = FROMGENESIS;
        consensus.SegwitHeight = FROMGENESIS;
        consensus.DIP0001Height = FROMGENESIS;
        consensus.DIP0003Height = consensus.nFirstPoSBlock;
        consensus.DIP0003EnforcementHeight = 1048576;
        consensus.DIP0003EnforcementHash = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
        consensus.DIP0008Height = consensus.nFirstPoSBlock * 4;
        consensus.powLimit = uint256S("0000ffff00000000000000000000000000000000000000000000000000000000");
        consensus.posLimit = uint256S("007ffff000000000000000000000000000000000000000000000000000000000");
        consensus.nPowTargetTimespan = 300;
        consensus.nPowTargetSpacing = 60;
        consensus.nPosTargetTimespan = consensus.nPowTargetTimespan / 2;
        consensus.nPosTargetSpacing = consensus.nPowTargetSpacing;
        consensus.nStakeMinAge = 15 * 60;
        consensus.nStakeMaxAge = 60 * 60 * 24 * 30;
        consensus.nMinStakeAmount = 0.5 * COIN;
        consensus.nMinStakeHistory = 15;
        consensus.nModifierInterval = 60;
        consensus.nCoinbaseMaturity = 15;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999;   // December 31, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nWindowSize = 100;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nThreshold = 50;

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1572790000; // Nov 1st, 2019
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1604180000;   // Nov 1st, 2020
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nWindowSize = 100;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nThreshold = 50;

        // Deployment of DIP0001
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nWindowSize = 100;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nThreshold = 50;

        // Deployment of DIP0003
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].bit = 3;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nWindowSize = 100;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nThreshold = 50;

        // Deployment of DIP0008
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].bit = 4;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nWindowSize = 100;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nThreshold = 10;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0000000000000000000000000000000000000000000000000000000000000000");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0000000000000000000000000000000000000000000000000000000000000000");

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0x1f;
        pchMessageStart[1] = 0x2f;
        pchMessageStart[2] = 0x1f;
        pchMessageStart[3] = 0x2f;
        nDefaultPort = 49152;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 1;
        m_assumed_chain_state_size = 0.5;
        nMaxReorganizationDepth = 100;

        // genesis
        uint32_t nTime = 1582819900;
        uint32_t nNonce = 66908;
        genesis = CreateGenesisBlock(nTime, nNonce, 0x1f00ffff, 1, 0 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,80);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,142);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,208);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};
        bech32_hrp = "zenx";
        nExtCoinType = 5;

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        // long living quorum params
        consensus.llmqs[Consensus::LLMQ_5_60] = llmq5_60;
        consensus.llmqs[Consensus::LLMQ_50_60] = llmq50_60;
        consensus.llmqs[Consensus::LLMQ_400_60] = llmq400_60;
        consensus.llmqs[Consensus::LLMQ_400_85] = llmq400_85;
        consensus.llmqTypeChainLocks = Consensus::LLMQ_5_60;
        consensus.llmqTypeInstantSend = Consensus::LLMQ_5_60;

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fRequireRoutableExternalIP = true;
        fMineBlocksOnDemand = false;
        fAllowMultipleAddressesFromGroup = true;
        fAllowMultiplePorts = true;

        nPoolMinParticipants = 3;
        nPoolMaxParticipants = 5;
        nFulfilledRequestExpireTime = 60*60;
        vSporkAddresses = {"ZXoR9MgySWXAkoWvso96pcog5J7VXiGPHm"};
        nMinSporkKeys = 1;
        fBIP9CheckMasternodesUpgraded = true;
        consensus.fLLMQAllowDummyCommitments = false;
        m_is_test_chain = false;

        checkpointData = {{{ 0, consensus.hashGenesisBlock }}};
        chainTxData = ChainTxData {};
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams
{
public:
    explicit CRegTestParams(const ArgsManager& args) {
        strNetworkID = "regtest";

        consensus.nFirstPoSBlock = 100;
        consensus.nMasternodePaymentsStartBlock = 100000;
        consensus.nMasternodePaymentsIncreaseBlock = 158000;
        consensus.nMasternodePaymentsIncreasePeriod = 576*30;
        consensus.nInstantSendConfirmationsRequired = 6;
        consensus.nInstantSendKeepLock = 24;
        consensus.nInstantSendSigsRequired = 6;
        consensus.nInstantSendSigsTotal = 10;
        consensus.nBudgetPaymentsStartBlock = 328008;
        consensus.nBudgetPaymentsCycleBlocks = 16616;
        consensus.nBudgetPaymentsWindowBlocks = 100;
        consensus.nSuperblockStartBlock = 614820;
        consensus.nSuperblockStartHash = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
        consensus.nSuperblockCycle = 16616;
        consensus.nGovernanceMinQuorum = 10;
        consensus.nGovernanceFilterElements = 20000;
        consensus.nMasternodeCollateral = 5000 * COIN;
        consensus.nMasternodeMinimumConfirmations = 15;
        consensus.BIP34Height = consensus.nFirstPoSBlock;
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = consensus.nFirstPoSBlock;
        consensus.BIP66Height = consensus.nFirstPoSBlock;
        consensus.CSVHeight = FROMGENESIS;
        consensus.SegwitHeight = FROMGENESIS;
        consensus.DIP0001Height = 1048576; // TODO
        consensus.DIP0003Height = 1048576; // TODO
        consensus.DIP0003EnforcementHeight = 1048576; // TODO
        consensus.DIP0003EnforcementHash = uint256S("0000000000000000000000000000000000000000000000000000000000000000");
        consensus.DIP0008Height = consensus.nFirstPoSBlock * 4;
        consensus.powLimit = uint256S("0000ffff00000000000000000000000000000000000000000000000000000000");
        consensus.posLimit = uint256S("007fff0000000000000000000000000000000000000000000000000000000000");
        consensus.nPowTargetTimespan = 60;
        consensus.nPowTargetSpacing = 60;
        consensus.nPosTargetSpacing = consensus.nPowTargetSpacing;
        consensus.nPosTargetTimespan = consensus.nPowTargetTimespan;
        consensus.nStakeMinAge = 60 * 5;
        consensus.nStakeMaxAge = 60 * 60 * 24 * 30;
        consensus.nModifierInterval = 60 * 20;
        consensus.nCoinbaseMaturity = 20;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 50;
        consensus.nMinerConfirmationWindow = 100;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999;   // December 31, 2008

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1572790000; // Nov 1st, 2019
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1604180000;   // Nov 1st, 2020

        // Deployment of DIP0001
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nWindowSize = 100;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0001].nThreshold = 50;

        // Deployment of DIP0003
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].bit = 3;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nWindowSize = 100;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0003].nThreshold = 50;

        // Deployment of DIP0008
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].bit = 4;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nWindowSize = 100;
        consensus.vDeployments[Consensus::DEPLOYMENT_DIP0008].nThreshold = 10;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0000000000000000000000000000000000000000000000000000000000000000");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0000000000000000000000000000000000000000000000000000000000000000");

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0x1f;
        pchMessageStart[1] = 0xe4;
        pchMessageStart[2] = 0x9a;
        pchMessageStart[3] = 0xf1;
        nDefaultPort = 8333;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 1;
        m_assumed_chain_state_size = 0.5;
        nMaxReorganizationDepth = 100;

        // genesis
        uint32_t nTime = 1559000000;
        uint32_t nNonce = 1;

        genesis = CreateGenesisBlock(nTime, nNonce, 0x1f07fff0, 1, 0 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,80);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,142);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,208);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};
        bech32_hrp = "zenx";
        nExtCoinType = 5;

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        // long living quorum params
        consensus.llmqs[Consensus::LLMQ_50_60] = llmq50_60;
        consensus.llmqs[Consensus::LLMQ_400_60] = llmq400_60;
        consensus.llmqs[Consensus::LLMQ_400_85] = llmq400_85;
        consensus.llmqTypeChainLocks = Consensus::LLMQ_50_60;
        consensus.llmqTypeInstantSend = Consensus::LLMQ_50_60;

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fRequireRoutableExternalIP = true;
        fMineBlocksOnDemand = false;
        fAllowMultipleAddressesFromGroup = false;
        fAllowMultiplePorts = false;

        nPoolMinParticipants = 3;
        nPoolMaxParticipants = 5;
        nFulfilledRequestExpireTime = 60*60;
        vSporkAddresses = {"ZXoR9MgySWXAkoWvso96pcog5J7VXiGPHm"};
        nMinSporkKeys = 1;
        fBIP9CheckMasternodesUpgraded = true;
        consensus.fLLMQAllowDummyCommitments = false;
        m_is_test_chain = false;

        checkpointData = {{{ 0, consensus.hashGenesisBlock }}};
        chainTxData =    ChainTxData {};
    }

    /**
     * Allows modifying the Version Bits regtest parameters.
     */
    void UpdateVersionBitsParameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout) {
        consensus.vDeployments[d].nStartTime = nStartTime;
        consensus.vDeployments[d].nTimeout = nTimeout;
    }
    void UpdateActivationParametersFromArgs(const ArgsManager& args);
};

void CRegTestParams::UpdateActivationParametersFromArgs(const ArgsManager& args)
{
    if(gArgs.IsArgSet("-segwitheight")) {
        int64_t height = gArgs.GetArg("-segwitheight", consensus.SegwitHeight);

        if(height < -1 || height >= std::numeric_limits<int>::max()) {
            throw std::runtime_error(strprintf("Activation height %ld for segwit is out of valid range. Use -1 to disable segwit.", height));
        } else if(height == -1) {
            LogPrintf("Segwit disabled for testing\n");
            height = std::numeric_limits<int>::max();
        }

        consensus.SegwitHeight = static_cast<int>(height);
    }

    if(!args.IsArgSet("-vbparams")) return;

for(const std::string& strDeployment : args.GetArgs("-vbparams")) {
        std::vector<std::string> vDeploymentParams;
        boost::split(vDeploymentParams, strDeployment, boost::is_any_of(":"));

        if(vDeploymentParams.size() != 3) {
            throw std::runtime_error("Version bits parameters malformed, expecting deployment:start:end");
        }

        int64_t nStartTime, nTimeout;

        if(!ParseInt64(vDeploymentParams[1], &nStartTime)) {
            throw std::runtime_error(strprintf("Invalid nStartTime (%s)", vDeploymentParams[1]));
        }

        if(!ParseInt64(vDeploymentParams[2], &nTimeout)) {
            throw std::runtime_error(strprintf("Invalid nTimeout (%s)", vDeploymentParams[2]));
        }

        bool found = false;

        for(int j=0; j < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; ++j) {
            if(vDeploymentParams[0] == VersionBitsDeploymentInfo[j].name) {
                UpdateVersionBitsParameters(Consensus::DeploymentPos(j), nStartTime, nTimeout);
                found = true;
                LogPrintf("Setting version bits activation parameters for %s to start=%ld, timeout=%ld\n", vDeploymentParams[0], nStartTime, nTimeout);
                break;
            }
        }

        if(!found) {
            throw std::runtime_error(strprintf("Invalid deployment (%s)", vDeploymentParams[0]));
        }
    }
}

static std::unique_ptr<const CChainParams> globalChainParams;

const CChainParams &Params()
{
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string& chain)
{
    ///////////////////////////////////////////////////////////
    return std::unique_ptr<CChainParams>(new CTestNetParams());

    ///////////////////////////////////////////////////////////
    if(chain == CBaseChainParams::MAIN)
        return std::unique_ptr<CChainParams>(new CMainParams());
    else if(chain == CBaseChainParams::TESTNET)
        return std::unique_ptr<CChainParams>(new CTestNetParams());
    else if(chain == CBaseChainParams::REGTEST)
        return std::unique_ptr<CChainParams>(new CRegTestParams(gArgs));

    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}
