// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "RaycoinViewer.h"
#include "blake2.h"
#include <primitives/block.h>
#include <hash.h>
#include <tinyformat.h>
#include <util/strencodings.h>
#include <crypto/common.h>
#include <streams.h>
#include <arith_uint256.h>
#include <sync.h>

CCriticalSection cs_hash;

uint256 toUint256(RaycoinViewer::Hash& hash)
{
    uint256 res;
    for (int i = 0; i < res.size(); ++i) res.begin()[i] = hash[hash.size()-1-i/4] >> ((i%4)*8) & 0xFF;
    return res;
}

RaycoinViewer::Hash toHash(uint256& hash)
{
    RaycoinViewer::Hash res;
    for (int i = 0; i < res.size(); ++i) res[i] =   hash.begin()[hash.size()-1-i*4] << 24 |
                                                    hash.begin()[hash.size()-2-i*4] << 16 |
                                                    hash.begin()[hash.size()-3-i*4] << 8 |
                                                    hash.begin()[hash.size()-4-i*4];
    return res;
}

uint256 CBlockHeader::GetHash() const
{
    return GetHash(true).second;
}

std::pair<bool, uint256> CBlockHeader::GetHash(bool verify) const
{
    CBlockHeader tmp = *this;
    tmp.nNonce &= 0xFFF; //zero out screen coordinates in nonce
    CDataStream header(SER_GETHASH, PROTOCOL_VERSION);
    header << tmp;
    RaycoinViewer::Hash seed;
    blake2(seed.data(), seed.size()*sizeof(uint32_t), header.data(), header.size(), nullptr, 0);

    LOCK(cs_hash);
    RaycoinViewer::inst.seed(seed);
    RaycoinViewer::inst.target(toHash(ArithToUint256(arith_uint256().SetCompact(nBits))));

    RaycoinViewer::TraceResult bestSave;
    bool bestHashSave;
    if (verify)
    {
        bestHashSave = RaycoinViewer::inst.bestHash();
        bestSave = RaycoinViewer::inst.traceResult();
        RaycoinViewer::inst.bestHash(false);
        RaycoinViewer::inst.resetBestHash();
        RaycoinViewer::inst.verifyPos({nNonce >> 22, nNonce >> 12 & 0x3FF});
    }
    else
        RaycoinViewer::inst.verifyPos({-1,-1});
    
    RaycoinViewer::inst.update();

    auto& res = RaycoinViewer::inst.traceResult();
    bool valid = res.success;
    uint256 resHash = toUint256(res.hash);
    if (verify)
    {
        if (!valid) resHash = toUint256(seed);
        res = bestSave;
        RaycoinViewer::inst.bestHash(bestHashSave);
    }
    return std::make_pair(valid, resHash);
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=0x%08x, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
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
