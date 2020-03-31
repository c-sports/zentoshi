// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/block.h>

#include <hash.h>
#include <streams.h>
#include <tinyformat.h>
#include <crypto/common.h>
#include <crypto/balloon.h>

uint256 CBlockHeader::GetHash() const
{
    return SerializeHash(*this);
}

uint256 CBlockHeader::GetPoWHash() const
{
    uint256 thash;
    std::vector<unsigned char> vch(80);
    CVectorWriter ss(SER_NETWORK, PROTOCOL_VERSION, vch, 0);
    ss << *this;
    balloon((char*)vch.data(), &thash);
    return thash;
}

bool CBlock::IsProofOfStake() const
{
    return (vtx.size() > 1 && vtx[1]->IsCoinStake());
}

bool CBlock::IsProofOfWork() const
{
    return !IsProofOfStake();
}

std::string CBlockHeader::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, type=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, prevoutStake=%s)\n",
        GetHash().ToString(),
        nNonce == 0 ? "PoS" : "PoW",
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        prevoutStake.ToString());
    return s.str();
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, type=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
        IsProofOfStake() ? "PoS" : "PoW",
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}
