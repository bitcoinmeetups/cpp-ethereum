/*
	This file is part of cpp-ethereum.

	cpp-ethereum is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
*/
/** @file BlockChain.cpp
 * @author Gav Wood <i@gavwood.com>
 * @date 2014
 */

#include "Common.h"
#include "RLP.h"
#include "Exceptions.h"
#include "Dagger.h"
#include "BlockInfo.h"
#include "BlockChain.h"
using namespace std;
using namespace eth;

BlockChain::BlockChain()
{
	ldb::Options o;
	auto s = ldb::DB::Open(o, "blockchain", &m_db);

	// Initialise with the genesis as the last block on the longest chain.
	m_lastBlockHash = m_genesisHash = BlockInfo::genesis().hash;
	m_genesisBlock = BlockInfo::createGenesisBlock();
}

BlockChain::~BlockChain()
{
}

u256s BlockChain::blockChain() const
{
	// TODO: return the current valid block chain from most recent to genesis.
	// TODO: arguments for specifying a set of early-ends
	u256s ret;

	return ret;
}

void BlockChain::import(bytes const& _block)
{
	BlockInfo bi;
	try
	{
		// VERIFY: populates from the block and checks the block is internally coherent.
		bi.populate(&_block);
		bi.verifyInternals(&_block);

		auto newHash = eth::sha3(_block);

		// Check block doesn't already exist first!
		if (m_numberAndParent.count(newHash))
			return;

		// Work out its number as the parent's number + 1
		auto it = m_numberAndParent.find(bi.parentHash);
		if (it == m_numberAndParent.end())
			// We don't know the parent (yet) - discard for now. It'll get resent to us if we find out about its ancestry later on.
			return;
		bi.number = it->second.first + 1;

		// Check Ancestry:
		// Check timestamp is after previous timestamp.
		if (bi.timestamp <= BlockInfo(block(bi.parentHash)).timestamp)
			throw InvalidTimestamp();

		// TODO: check difficulty is correct given the two timestamps.
//		if (bi.timestamp )

		// TODO: check transactions are valid and that they result in a state equivalent to our state_root.
		// this saves us from an embarrassing exit later.

		// Check uncles.
		for (auto const& i: RLP(_block)[2])
		{
			auto it = m_numberAndParent.find(i.toInt<u256>());
			if (it == m_numberAndParent.end())
				return;	// Don't (yet) have the uncle in our list.
			if (it->second.second != bi.parentHash)
				throw InvalidUncle();
		}

		// Insert into DB
		m_numberAndParent[newHash] = make_pair(bi.number, bi.parentHash);
		m_children.insert(make_pair(bi.parentHash, newHash));
		ldb::WriteOptions o;
		m_db->Put(o, ldb::Slice(toBigEndianString(newHash)), (ldb::Slice)ref(_block));

		// This might be the new last block; count back through ancestors to common shared ancestor and compare to current.
		// TODO: Use GHOST algorithm.
	}
	catch (...)
	{
		// Exit silently on exception(?)
		return;
	}
}

bytesConstRef BlockChain::block(u256 _hash) const
{
	if (_hash == m_genesisHash)
		return &m_genesisBlock;
	return &m_genesisBlock;
}

u256 BlockChain::lastBlockNumber() const
{
	if (m_lastBlockHash == m_genesisHash)
		return 0;

	return m_numberAndParent[m_lastBlockHash].first;
}