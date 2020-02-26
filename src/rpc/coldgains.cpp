// Copyright (c) 2019-2020 Zentoshi LLC
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <coldgains.h>

#include <core_io.h>
#include <key_io.h>
#include <rpc/request.h>
#include <rpc/server.h>
#include <univalue.h>
#include <util/moneystr.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <util/validation.h>
#include <validation.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>

UniValue listcoldparams(const JSONRPCRequest& request)
{
    //! list the parameters for coldgains for the current environment
    const Consensus::Params& consensusParams = Params().GetConsensus();
    std::vector<unsigned int> coldgainsParams = consensusParams.nColdGainParams;

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("minage", (int)coldgainsParams.at(ColdGains::MINAGE));
    obj.pushKV("maxage", (int)coldgainsParams.at(ColdGains::MAXAGE));
    obj.pushKV("minamt", (int)coldgainsParams.at(ColdGains::MINAMOUNT));
    obj.pushKV("maxamt", (int)coldgainsParams.at(ColdGains::MAXAMOUNT));
    obj.pushKV("interest", (int)coldgainsParams.at(ColdGains::INTEREST));
    return obj;
}

UniValue listcoldcandidates(const JSONRPCRequest& request)
{
    //! list valid input candidates for coldgains in the current environment
    const Consensus::Params& consensusParams = Params().GetConsensus();
    std::vector<unsigned int> coldgainsParams = consensusParams.nColdGainParams;
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    std::vector<COutput> vPossibleCoins;
    LOCK2(cs_main, pwallet->cs_wallet);
    pwallet->AvailableCoins(vPossibleCoins, true);

    UniValue obj(UniValue::VARR);
    for (COutput& out : vPossibleCoins) {
        UniValue inpObj(UniValue::VOBJ);
        int inputAge = GetCandidateInputAge(out.tx->GetHash());
        CAmount inputValue = out.tx->tx->vout[out.i].nValue;
        if (inputAge >= coldgainsParams.at(ColdGains::MINAGE) &&
            inputAge <= coldgainsParams.at(ColdGains::MAXAGE) &&
            inputValue >= (coldgainsParams.at(ColdGains::MINAMOUNT) * COIN) &&
            inputValue <= (coldgainsParams.at(ColdGains::MAXAMOUNT) * COIN)) {
            inpObj.pushKV("txid", out.tx->GetHash().ToString());
            inpObj.pushKV("vout", out.i);
            inpObj.pushKV("value", (inputValue / COIN));
            obj.push_back(inpObj);
        }
    }
    return obj;
}

UniValue createcoldtransaction(const JSONRPCRequest& request)
{
    std::shared_ptr<CWallet> const wallet = GetWalletForJSONRPCRequest(request);
    CWallet* const pwallet = wallet.get();

    //! validate params
    std::string txid = "";
    int vout = 0, blkduration = 0;
    if (!request.params[0].isNull() &&
        !request.params[0].get_str().empty() &&
        !request.params[1].isNull() &&
        !request.params[2].isNull()) {
        txid = request.params[0].get_str();
        vout = request.params[1].get_int();
        blkduration = request.params[2].get_int();
    } else {
        return "failure";
    }

    //! see if it exists
    bool fSuccess = false;
    UniValue obj(UniValue::VOBJ);
    std::vector<COutput> vPossibleCoins;
    LOCK2(cs_main, pwallet->cs_wallet);
    pwallet->AvailableCoins(vPossibleCoins, true);
    for (COutput& out : vPossibleCoins) {
        if (strstr(out.tx->GetHash().ToString().c_str(),txid.c_str()) && out.i == vout) {
            CScript coldScriptDest = out.tx->tx->vout[out.i].scriptPubKey;
            const CAmount& coldScriptAmount = out.tx->tx->vout[out.i].nValue;
            if (createColdGainTransaction(out, coldScriptDest, blkduration, coldScriptAmount)) {
                fSuccess = true;
                break;
            }
        }
    }

    return fSuccess ? "success" : "failure";
}

static const CRPCCommand commands[] =
    {
        //  category              name                      actor (function)         okSafeMode
        //  --------------------- ------------------------  -----------------------  ----------
        {   "coldgains",          "listcoldparams",         &listcoldparams,         {}},
        {   "coldgains",          "listcoldcandidates",     &listcoldcandidates,     {}},
        {   "coldgains",          "createcoldtransaction",  &createcoldtransaction,  {"txid", "vout", "duration"}},
};

void RegisterColdGainsRPCCommands(CRPCTable& tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++) {
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
    }
}
