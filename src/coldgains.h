// Copyright (c) 2019-2020 Zentoshi LLC
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key_io.h>
#include <script/script.h>
#include <util/system.h>
#include <uint256.h>

class COutput;

namespace ColdGains {
   enum Params {
      MINAGE,
      MAXAGE,
      MINAMOUNT,
      MAXAMOUNT,
      INTEREST
   };
}

unsigned int GetCandidateInputAge(uint256 txHash);
bool createColdGainTransaction(COutput& out, CScript& gainsAddress, const int64_t blockHeight, const CAmount origValue);
bool checkColdGainTransaction(const CTransaction& tx, const Consensus::Params& consensusParams);
