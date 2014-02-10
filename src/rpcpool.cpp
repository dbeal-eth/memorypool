#include "main.h"
//#include "prime.h"
#include "db.h"
#include "init.h"
#include "bitcoinrpc.h"

#include "pool.h"

#include <list>

//#include "blockpayment.h"

using namespace json_spirit;
using namespace std;

//int64 GetBlockValue(int nBits, int64 nFees);

Value gethashrate(const Array& params, bool fHelp)
{
	if(fHelp || params.size() != 0)
		throw runtime_error(
			"gethashrate\n"
			"Returns a measurement of the pool's hashrate");

	return sharerate * 16; // * 16 because its one out of every 16 shares on average
}

Value getnumclients(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getnumclients\n"
            "Returns the number of clients on the pool");

    //CPoolClient::clientsLock.lock();
    int clients = (int)CPoolClient::clients.size();
    //CPoolClient::clientsLock.unlock();
    return clients;
}

Value getroundshareinfo(const Array& params, bool fHelp) {
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getroundshareinfo\n"
            "Returns information about shares for the current round");

    //poolQueueLock.lock();
    int numShares = poolQueue.getCurrent()->getRunningTotal();
    //poolQueueLock.unlock();
    return numShares;
}

Value getfoundblocks(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getfoundblocks\n"
            "Returns the block numbers that have been found (probobly for last 3 days)");
	
	Array foundBlocks;
	BlockPayment::Nodes::Block* cur = poolQueue.peek();
	int i = 0;
	while(cur != NULL && i < 7000) {
		i++;
		if(cur->getHeight() == 0) continue;
		Object block;
		block.push_back(Pair("height", (int)cur->getHeight()));
		block.push_back(Pair("amount", cur->getXPM()));
		block.push_back(Pair("shares", (int)cur->getRunningTotal()));
		block.push_back(Pair("time", (long)FindBlockByHeight(cur->getHeight())->nTime));
		block.push_back(Pair("confirmations", (int)cur->getHeight() + PAYOUT_DELAY - (int)pindexBest->nHeight));
		foundBlocks.push_back(block);
		cur = cur->getNext();
	}
		
	// reverse the order of found blocks so it is chronologically backwords
	Array res;
	for(int i = foundBlocks.size() - 1;i >= 0;i--) res.push_back(foundBlocks[i]);
	
    return res;
}

// Potentially could be expanded to list shares
Value listtopminers(const Array& params, bool fHelp)
{
	if(fHelp) throw runtime_error("listtopminers\nReturns the best miners for the current block");
	
	BlockPayment::Nodes::Block* cur = poolQueue.getCurrent();
	//Object miners;
	
	// Parse Users
	BlockPayment::Nodes::User* user = cur->getUserList();
	
	std::list<BlockPayment::Nodes::User *> miners;	
	std::list<BlockPayment::Nodes::User *>::iterator  it;
	miners.push_back(user);
	user = user->next;
	while(user != NULL) {
		it = miners.begin();
		bool adv = false;
		while((*it)->runningtotal < user->runningtotal && it != miners.end()) {
			++it;
			adv = true;
		}
		if(!adv) miners.push_back(user);
		else {
			++it;
			miners.insert(it, user);
		}
		user = user->next;
	}
	Object ret;
	for(it = miners.begin();it != miners.end();++it) {
		ret.push_back(Pair((*it)->username, (int)(*it)->runningtotal));
	}
	return ret;
}

Value listpayouts(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "listpayouts <block #>\n"
            "Returns the payouts to each address for this block");

	unsigned int bnum = atoi(params[0].get_str().c_str());
	
	BlockPayment::Nodes::Block* cur = poolQueue.peek();
	while(cur != NULL) {
		if(cur->getHeight() != bnum) {
			cur = cur->getNext();
			continue;
		}
		if(!cur->hasCalculatedXPM()) cur->calculateXPMPerUser();
		// return stuff
		Object payouts;
		BlockPayment::Nodes::User* user = cur->getUserList();
		while(user != NULL) {
			payouts.push_back(Pair(user->username, user->XPM));
			user = user->next;
		}
		return payouts;
	}
    return 0;
}

/*Value getusertransactions(const Array& params, bool fHelp) {
	if(fHelp || params.size() != 1) throw runtime_error(
		"getusertransactions\n"
		"Lists the transactions related to a specific user");
	
	std::string user = params[0].get_str();
	
	CBitcoinAddress address(user);
	if(!address.IsValid()) return 0;
	std::list<CAccountingEntry> acents;
	CWallet::TxItems txOrdered = pwalletMain->OrderedTxItems(acents, "");
	
	Array ret;

	for (CWallet::TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it) {
		CWalletTx *const pwtx = (*it).second.first;
		std::list<std::pair<CTxDestination, int64> > recieved;
		std::list<std::pair<CTxDestination, int64> > sent;
		int64 fee;
		std::string account;
		pwtx->GetAmounts(recieved, sent, fee, account);
		BOOST_FOREACH(const PAIRTYPE(CTxDestination, int64)& s, recieved) {
			if(CBitcoinAddress(s.first).ToString() == user) {
				// Found a transaction to, return information
				Object txn;
				txn.push_back(Pair("id", pwtx->GetHash().GetHex()));
				txn.push_back(Pair("amount", ValueFromAmount(s.second)));
				ret.push_back(txn);
				break;
			}
		}
	}
	return 0;
}*/

Value getuserinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getuserinfo\n"
            "Returns various mining information related to the specified user");
    
    std::string user = params[0].get_str();
 	//user = user.replace("'", "");
	//user = user.replace('"', "");   
	CBitcoinAddress address(user);
	if (!address.IsValid()) return 0;
	Object uinfo;
	BlockPayment::Nodes::User *u = poolQueue.getCurrent()->getUser(user);
	uinfo.push_back(Pair("balance", userInfos[user].XPM));
	uinfo.push_back(Pair("unconfirmed", poolQueue.getUsersUnconfirmedBalance(user)));
	if(u != NULL) {
		uinfo.push_back(Pair("roundShares", (int)u->runningtotal));
		uint64 mint;
		//TargetGetMint(pindexBest->nBits, mint);
		mint = 280 * COIN;
		uinfo.push_back(Pair("estimatedPayout", ((double)u->runningtotal / (double)poolQueue.getCurrent()->getRunningTotal()) * (mint / COIN)));
	}
	uinfo.push_back(Pair("foreverShares", (int)0));
	//poolQueueLock.unlock();
    return uinfo;
}
