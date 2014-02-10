#include "pool.h"

#include <boost/bind.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>
//#include <boost/assign/list_of.hpp>

#include <dirent.h>

#include "bignum.h"
#include "main.h"
#include "bitcoinrpc.h"
#include "util.h"

#include "json/json_spirit.h"


#include <iostream>
#include <fstream>
#include <string>
#include <unistd.h>
// TODO: Potential windows support if it will become open source

#include "helper.h"

#define BOOST_ASIO_ENABLE_HANDLER_TRACKING

//#include "xpt.h"

//#define BOOST_ASIO_ENABLE_HANDLER_TRACKING

boost::shared_mutex poolQueueLock;
BlockPayment::Queue poolQueue;

boost::shared_mutex CPoolClient::clientsLock;
std::vector<CPoolClient *> CPoolClient::clients;

std::map<std::string, BlockPayment::Nodes::User> userInfos;

const char* BLOCKS_LOCATION = "/pool/blocks/";
char* BLOCKS_COMPUTED = "";

float sharerate = 0; // result of a simple calculation of shares on the server, see PoolSaveThread()

const int PAYOUT_DELAY = 80; // Change this to set number of blocks to confirm before payout

//CPoolServer *gPoolServer;

int64 GetAccountBalance(const string& strAccount, int nMinDepth);

// Pulled straight from xolokram's mining client
void convertDataToBlock(unsigned char* blockData, CBlock& block) {
	
	block.nVersion               = *((int *)(blockData));
	block.hashPrevBlock          = *((uint256 *)(blockData + 4));
	block.hashMerkleRoot         = *((uint256 *)(blockData + 36));
	block.nTime                  = *((unsigned int *)(blockData + 68));
	block.nBits                  = *((unsigned int *)(blockData + 72));
	block.nNonce                 = *((unsigned int *)(blockData + 76));
	block.nBirthdayA             = *((unsigned int *)(blockData + 80));
	block.nBirthdayB             = *((unsigned int *)(blockData + 84));
	
	//std::vector<unsigned char> vch;
	//for(int i = 0;i < *((unsigned char *)(blockData + 80));i++) vch.push_back(*((unsigned char *)(blockData + 81 + i)));
	//block.bnPrimeChainMultiplier = CBigNum(vch);
}

void convertBlockToData(CBlock& block, unsigned char* data) {
	memcpy(data, &block.nVersion, 4);
	memcpy(data + 4, &block.hashPrevBlock, 32);
	memcpy(data + 36, &block.hashMerkleRoot, 32);
	memcpy(data + 68, &block.nTime, 4);
	memcpy(data + 72, &block.nBits, 4);
	memcpy(data + 76, &block.nNonce, 4);
	// No need to copy anything else
}

void saveCurrentBlock() {
	//poolQueueLock.lock();
	//std::cout << "Saving: " << std::string(BLOCKS_COMPUTED) << "current" << std::endl;
	{
		boost::unique_lock<boost::shared_mutex> lock(poolQueueLock);
		poolQueue.save(std::string(BLOCKS_COMPUTED) + "current");
	}
	//poolQueueLock.unlock();
}

bool sendPayouts(std::vector<std::pair<CScript, int64> > payouts, CWallet* pwallet, std::string account) {
	EnsureWalletIsUnlocked();
	// Check funds
	//int64 nBalance = GetAccountBalance(account, 1);
	
	int64 total = 0;
	for(int i = 0;i < payouts.size();i++) {
		total += (payouts[i].second);
	}
	printf("[POOl] AMOUNT: %i\n", total);
	
	//if (total > nBalance) {
	//	printf("[POOL] Pool has insufficient funds!!! (%i > %i)\n", total, nBalance);
	//	return false;
		// This prevents other errors
	//}

	// Send
	printf("[POOL] Trying to send payouts...\n");
	CWalletTx wtx;
	CReserveKey keyChange(pwallet);
	int64 nFeeRequired = 0;
	std::string strFailReason;
	bool fCreated = pwallet->CreateTransaction(payouts, wtx, keyChange, nFeeRequired, strFailReason);
	if (!fCreated) {
		printf("Failed to create transaction for payouts: %s\n", strFailReason.c_str());
		return false;
	}
	if (!pwallet->CommitTransaction(wtx, keyChange)) {
		printf("Failed to commit transaction for payouts!");
		return false;
	}
	printf("Sent payouts: %s\n", wtx.GetHash().GetHex().c_str());
	return true;
}

/*void saveUserInfos() {
	json_spirit::Object infos;
	BlockPayment::Nodes::User* i = userInfos;
	while(i != NULL)  {
		infos.push_back(json_spirit::Pair(i->username, i->XPM));
		i = i->next;
	}
	std::ofstream out((std::string(BLOCKS_COMPUTED) + "../users").c_str());
	json_spirit::write(infos, out);
	out.close();
}*/

void saveUserInfos() {
	json_spirit::Object infos;
	for(std::map<std::string, BlockPayment::Nodes::User>::iterator it = userInfos.begin();it != userInfos.end();++it) {
		infos.push_back(json_spirit::Pair(it->first, it->second.XPM));
	}
	std::ofstream out((std::string(BLOCKS_COMPUTED) + "../users").c_str());
	json_spirit::write(infos, out);
	out.close();
}

void addBalance(std::string address, double amount) {
	/*if(userInfos == NULL) {
		userInfos = new BlockPayment::Nodes::User(address, 0);
		userInfos->XPM += amount;
		return;
	}
	
	BlockPayment::Nodes::User* i = userInfos; //iterator
    if(address < i->username) {
        BlockPayment::Nodes::User* t = i;
        i = new BlockPayment::Nodes::User(address, 0);
        i->XPM += amount;
        i->next = t;
        userInfos = i;
        return;
    }
    else if(address == i->username) {
        i->XPM += amount;
        return;
    }
    else if(i->next == NULL) {
        i->next = new BlockPayment::Nodes::User(address, 0);
        i->next->XPM = amount;
        return;
    }
    while((address > i->next->username) && (i->next->next != NULL)) {
        i = i->next;
    }
    if(address < i->next->username) {
        BlockPayment::Nodes::User* t = i->next;
        i->next = new BlockPayment::Nodes::User(address, 0);
        i->next->XPM = amount;
        i->next->next = t;
    }
    else if(address == i->next->username) {
        i->next->XPM += amount;
    }
    else {
        i->next->next = new BlockPayment::Nodes::User(address, 0);
        i->next->next->XPM = amount;
    }*/
    
    userInfos[address].XPM += amount;
}

void loadUserInfos() {
	json_spirit::Value i;
	std::ifstream in((std::string(BLOCKS_COMPUTED) + "../users").c_str());
	if(!in.is_open()) return; // No users yet
	json_spirit::read(in, i);
	in.close();
	json_spirit::Object infos = i.get_obj();
	for(unsigned int i = 0;i < infos.size();i++) {
		//addBalance(infos[i].name_, infos[i].value_.get_real());
		userInfos[infos[i].name_].XPM = infos[i].value_.get_real();
		userInfos[infos[i].name_].username = infos[i].name_;
	}
}

void processBalances(unsigned int height, CWallet *pwallet) {
	//poolQueueLock.lock();
	{
	boost::unique_lock<boost::shared_mutex> lock(poolQueueLock);
	if(poolQueue.peek() != NULL) {
		//std::vector<boost::pair<CScript, int64> > payouts;
		while(poolQueue.peek()->getHeight() < height - PAYOUT_DELAY && poolQueue.peek()->getHeight() != 0) {
			BlockPayment::Nodes::Block* block = poolQueue.deq();
			if(!block->hasCalculatedXPM()) block->calculateXPMPerUser();
			BlockPayment::Nodes::User* user = block->getUserList();
			while(user != NULL) {
				addBalance(user->username, user->XPM);
				user = user->next;
			}
			saveUserInfos();
			// Move the file to the oldblocks location so we dont double spend
			boost::filesystem::rename(std::string(std::string(BLOCKS_COMPUTED) + helper::uintToString(block->getHeight())).c_str(), std::string(std::string(BLOCKS_COMPUTED) + "../oldblocks/" + helper::uintToString(block->getHeight())).c_str());
			delete block;
		}
	}
	}
	//poolQueueLock.unlock();
	poolQueue.updateUnconfirmedUserBalances();
	
	if(height % 5 == 0) { // Execute every 5 blocks
		printf("[POOL] Executing payouts...\n");
		// Find out who needs to be payed and send their payouts
		std::vector<std::pair<CScript, int64> > payments;
		std::vector<std::string> payed;
		for(std::map<std::string, BlockPayment::Nodes::User>::iterator it = userInfos.begin();it != userInfos.end();++it) {
			if(it->second.XPM >= BlockPayment::MIN_PAYOUT) {
				// Generate and double check the address
				CBitcoinAddress address(it->second.username);
				if (!address.IsValid()) {
					std::cout << "Invalid primecoin address: " << it->second.username << std::endl;
					continue;
				}
				// Generate the cscript
				CScript script;
				script.SetDestination(address.Get());
				// Make the payment
				payments.push_back(std::make_pair(script, (long)round(it->second.XPM * COIN)));
				payed.push_back(it->second.username);
			}
		}
		if(payments.empty()) return;
		if(sendPayouts(payments, pwallet, "")) {
			// Reset all the affected balances to 0
			for(int i = 0;i < payed.size();i++) {
				userInfos[payed[i]].XPM = 0;
			}
		}
		else printf("[POOL] FAILED to send payouts!");
	}
}

void CPoolClient::Run(boost::shared_ptr<tcp::socket> sock) {
	if(!sock->is_open()) return;
	this->sock = sock;
	const int max_length = 256;
	try {
		// Get initial hello
		unsigned char *data = new unsigned char[max_length];
		boost::system::error_code error;
		int red = sock->read_some(boost::asio::buffer(data, max_length), error);
		if(error || red < 1)
			throw boost::system::system_error(error); // Should not close connection before hello
		/*if(!xpt) {*/
			// Process hello
			char length  = *((unsigned char *)(data));
			if(red < length + 1 + 8) return;
			char *a = new char[length + 1];
			memset(a, 0, length + 1);
			memcpy(a, data + 1, length);
			address = a;
			delete[] a;
			majorVersion    = *((unsigned char *)(data + length + 2));
			minorVersion    = *((unsigned char *)(data + length + 3));
			payload         = 1;
			fee             = *((unsigned char *)(data + length + 5));
			minerId         = *((unsigned short *)(data + length + 6));
			//sieveExtensions = *((unsigned int *)(data + length + 8));
			//sievePercentage = *((unsigned int *)(data + length + 12));
			//sieveSize       = *((unsigned int *)(data + length + 16));
			// Ignore all past here (the password
		/*} else {
			xptPacketbuffer_t *packet = parsePacket(data);
			//std::cout << "BUFFER SIZE: " << packet->bufferSize << std::endl;
			//std::cout << "INDEX: " << packet->parserIndex << std::endl;
			bool error;
			xptPacketbuffer_beginReadPacket(packet);
			uint32_t version = xptPacketbuffer_readU32(packet, &error);
			char *a = new char[128];
			xptPacketbuffer_readString(packet, a, 128, &error);
			char *p = new char[128];
			xptPacketbuffer_readString(packet, p, 128, &error);
			//std::cout << "INDEX: " << packet->parserIndex << std::endl;
			payload = xptPacketbuffer_readU32(packet, &error);
			
			if(error) {
				printf("[POOL] Client error... could not connect!\n");
				return;
			}

			if(payload < 1) payload = 1;
			if(payload > 128) payload = 127;
			address = a;
			
			// After reading, delete a and p
			delete[] a;
			delete[] p;
			xptPacketbuffer_free(packet);
			
			// Send authentication acknowledgement
			packet = xptPacketbuffer_create(64 * 1024);
			error = false;
			xptPacketbuffer_beginWritePacket(packet, XPT_OPC_S_AUTH_ACK);
			xptPacketbuffer_writeU32(packet, &error, 0);
			xptPacketbuffer_writeString(packet, (char *)"", 512, &error);
			xptPacketbuffer_finalizeWritePacket(packet);
			boost::asio::write(*sock, boost::asio::buffer((unsigned char *)packet->buffer, packet->parserIndex));
			xptPacketbuffer_free(packet);
		} */
		
		//std::cout << "Client successfully connected: " << address << (xpt ? " (XPT)" : "")  << " Payload: " << (int)payload << std::endl;
		printf("[POOL] Client successfully connected: %s Payload: %i\n", address.c_str(), (int)payload);
		for(int i = 0;i < payload;i++) {
			KeyBlockPair kbp(wallet);
			keys.push_back(kbp);
		}
		Sendwork();
		//clientsLock.lock();
		{
			boost::unique_lock<boost::shared_mutex> lock(clientsLock);
			clients.push_back(this);
			boost::asio::async_read(*sock, boost::asio::buffer(buf, 88),boost::bind(&CPoolClient::HandleSubmission, this,boost::asio::placeholders::error));
		}
		//clientsLock.unlock();
	} catch(boost::system::system_error& e) {
		std::cout << "CLIENT ERROR: " << e.what() << std::endl;
	} catch(std::exception& e) {
		std::cout << "STD EXCEPTION: " << e.what() << std::endl;
	}
}

void CPoolClient::Sendwork() {
	try {
		boost::shared_ptr<CBlockTemplate> pblocktemplate(CreateNewBlock(keys[0].key));
		if (!pblocktemplate.get()) {
			printf("Could not get block template!\n");
			return;
		}
		CBlock *pblock = &pblocktemplate->block;
		IncrementExtraNonce(pblock, pindexBest, extraNonce);
		unsigned char data[89];
		//if(!xpt) {
			keys[0].block = *pblock;
			data[0] = 0;
			convertBlockToData(*pblock, &data[1]);
			boost::asio::write(*sock, boost::asio::buffer(data, 89));
		/*}
		else {
			xptPacketbuffer_t* p = xptPacketbuffer_create(64 * 1024);
			bool error = false;
			xptPacketbuffer_beginWritePacket(p, XPT_OPC_S_WORKDATA1);
			
			xptPacketbuffer_writeU32(p, &error, pblock->nVersion);
			xptPacketbuffer_writeU32(p, &error, pindexBest->nHeight);
			xptPacketbuffer_writeU32(p, &error, pblock->nBits);
			xptPacketbuffer_writeU32(p, &error, BlockPayment::MIN_CHAIN_LENGTH << 24);
			xptPacketbuffer_writeU32(p, &error, pblock->nTime);
			xptPacketbuffer_writeData(p, (unsigned char *)&pblock->hashPrevBlock, 32, &error);
			xptPacketbuffer_writeU32(p, &error, payload);
			
			xptPacketbuffer_writeData(p, (unsigned char *)&pblock->hashMerkleRoot, 32, &error);
			keys[0].block = pblocktemplate->block;
			for(int i = 1;i < payload;i++) {
				boost::shared_ptr<CBlockTemplate> pbt(CreateNewBlock(keys[i].key));
				if (!pbt.get()) {
					printf("[POOL] Could not get block template!");
					return;
				}
				// Add the merkle root
				IncrementExtraNonce(&pbt->block, pindexBest, extraNonce);
				xptPacketbuffer_writeData(p, (unsigned char *)&pbt->block.hashMerkleRoot, 32, &error);
				//std::cout << "Wrote " << pbt->block.hashMerkleRoot.GetHex() << std::endl;
				keys[i].block = pbt->block;
			}
			xptPacketbuffer_finalizeWritePacket(p);
			boost::asio::write(*sock, boost::asio::buffer((unsigned char *)p->buffer, p->parserIndex));
			xptPacketbuffer_free(p);
		}*/
		//cblock = *pblock; // For checking purposes
		submitted.clear();
	} catch(boost::system::system_error& e) {
		std::cout << "CLIENT EXCEPTION: " << e.what() << std::endl;
	} catch(std::exception& e) {
		std::cout << "STD EXCEPTION: " << e.what() << std::endl;
	}
}

void CPoolClient::HandleSubmission(const boost::system::error_code& error) {
	try {
		if(!error) {
			CBlock block;
			/*if(!xpt) {*/
				convertDataToBlock(buf, block); // All 88 bytes should be there because not read_some
			/*}
			else {
				uint32_t packetDataSize = ((*((uint32_t *)(buf))) >> 8) & 0xFFFFFF;
				if( packetDataSize >= (1024*1024*2-4) )
				{
					// packets larger than 2mb are not allowed
					std::cout << "Client packet exceeds 2mb size limit" << std::endl;
					return; // TODO: Possibly actually close the connection
				}
				unsigned char data[packetDataSize + 4];
				memcpy(&data[0], buf, 4);
				boost::asio::read(*sock, boost::asio::buffer(&data[4], packetDataSize));
				// We should now have a complete packet, TODO: errors, now read
				xptPacketbuffer_t *p = parsePacket(data);
				
				unsigned int headerVal = *(unsigned int *)buf;
				unsigned int opcode = (headerVal & 0xFF);
				
				if(opcode == XPT_OPC_C_PING) {
					// TODO: Just respond because we should :)
					
					bool error = false;
					xptPacketbuffer_beginReadPacket(p);
					unsigned int version = xptPacketbuffer_readU32(p, &error);
					unsigned int low = xptPacketbuffer_readU32(p, &error);
					unsigned int high = xptPacketbuffer_readU32(p, &error);
					
					if(version == 2) {
						long time = ((long)high << 32) | (low);
						std::cout << "PING TIME: " << time << std::endl;
					} else {
						printf("[POOL] Unsupported ping type!\n");
					}
				}
				else if(opcode == XPT_OPC_C_SUBMIT_SHARE) {
					//std::cout << "XPT Packet read" << std::endl;
					bool error = false;
					xptPacketbuffer_beginReadPacket(p);
					xptPacketbuffer_readData(p, (unsigned char *)&block.hashMerkleRoot, 32, &error);
					xptPacketbuffer_readData(p, (unsigned char *)&block.hashPrevBlock, 32, &error);
					block.nVersion = xptPacketbuffer_readU32(p, &error);
					block.nTime = xptPacketbuffer_readU32(p, &error);
					block.nNonce = xptPacketbuffer_readU32(p, &error);
					block.nBits = xptPacketbuffer_readU32(p, &error);
					// Skip the stats
					xptPacketbuffer_readU32(p, &error);
					xptPacketbuffer_readU32(p, &error);
					// Skip teh fixed multiplier
					xptPacketbuffer_readU8(p, &error);
					//char size = xptPacketbuffer_readU8(p, &error);
					//unsigned char* fm = new unsigned char[size];
					//xptPacketbuffer_readData(p, fm, size, &error);
					
					//std::vector<unsigned char> vch;
					//for(int i = 0;i < size;i++) vch.push_back(*((unsigned char *)fm + i));
					//CBigNum fixed(vch);
					
					char size = xptPacketbuffer_readU8(p, &error);
					//std::cout << "SIZE: " << (int)size << std::endl;
					//std::cout << "CURSOR: " << p->parserIndex << std::endl;
					//std::cout << "BUFSIZE: " << p->bufferSize << std::endl;
					unsigned char* cm = new unsigned char[size];
					xptPacketbuffer_readData(p, cm, size, &error);
					
					std::vector<unsigned char> vch;
					for(int i = 0;i < size;i++) vch.push_back(*((unsigned char *)cm + i));
					//std::vector<unsigned char> tch = vch;
					//vch.clear();
					//for(int i = 0;i < size;i++) vch.push_back(tch[size - i - 1]);
					
					block.bnPrimeChainMultiplier = CBigNum(vch);
					
					//std::cout << "PRIME MULTIPLIER: " << block.bnPrimeChainMultiplier.GetHex() << std::endl;
					
					//if(error) std::cout << "ERROR" << std::endl;
					
					//delete[] fm;
					delete[] cm;
					/*
					 * 	xptPacketbuffer_beginWritePacket(xptClient->sendBuffer, XPT_OPC_C_SUBMIT_SHARE);
						xptPacketbuffer_writeData(xptClient->sendBuffer, xptShareToSubmit->merkleRoot, 32, &sendError);		// merkleRoot
						xptPacketbuffer_writeData(xptClient->sendBuffer, xptShareToSubmit->prevBlockHash, 32, &sendError);	// prevBlock
						xptPacketbuffer_writeU32(xptClient->sendBuffer, &sendError, xptShareToSubmit->version);				// version
						xptPacketbuffer_writeU32(xptClient->sendBuffer, &sendError, xptShareToSubmit->nTime);				// nTime
						xptPacketbuffer_writeU32(xptClient->sendBuffer, &sendError, xptShareToSubmit->nonce);				// nNonce
						xptPacketbuffer_writeU32(xptClient->sendBuffer, &sendError, xptShareToSubmit->nBits);				// nBits
						xptPacketbuffer_writeU32(xptClient->sendBuffer, &sendError, xptShareToSubmit->sieveSize);			// sieveSize
						xptPacketbuffer_writeU32(xptClient->sendBuffer, &sendError, xptShareToSubmit->sieveCandidate);		// sieveCandidate
						// bnFixedMultiplier
						xptPacketbuffer_writeU8(xptClient->sendBuffer, &sendError, xptShareToSubmit->fixedMultiplierSize);
						xptPacketbuffer_writeData(xptClient->sendBuffer, xptShareToSubmit->fixedMultiplier, xptShareToSubmit->fixedMultiplierSize, &sendError);
						// bnChainMultiplier
						xptPacketbuffer_writeU8(xptClient->sendBuffer, &sendError, xptShareToSubmit->chainMultiplierSize);
						xptPacketbuffer_writeData(xptClient->sendBuffer, xptShareToSubmit->chainMultiplier, xptShareToSubmit->chainMultiplierSize, &sendError);
						// finalize
						xptPacketbuffer_finalizeWritePacket(xptClient->sendBuffer);
					 
				}
				else {
					printf("[POOL] CLIENT ERROR: Undefined opcode!\n");
					return;
				}
				xptPacketbuffer_free(p);
			}*/
			
			// TODO: Right here, get smart and figure out which block the cblock is so we can do these checks!
			KeyBlockPair *kbp = NULL; // TODO: Could cause problems
			for(unsigned int i = 0;i < keys.size();i++) {
				if(keys[i].block.hashMerkleRoot == block.hashMerkleRoot) {
					kbp = &keys[i];
					break;
				}
			}
			
			bool valid = true;
			
			if(kbp == NULL) {
				// Not a valid share, we cant find the merkle root to match!
				printf("[POOL] Could not find merkle root in key list: %s\n", block.hashMerkleRoot.GetHex().c_str());
				//for(unsigned int i = 0;i < keys.size();i++) std::cout << keys[i].block.hashMerkleRoot.GetHex() << std::endl;
				valid = false;
			}
			
			int response = 0;
			// Check to make sure the submission is still current
			if(valid) {
				if(block.hashPrevBlock != kbp->block.hashPrevBlock) {
					printf("[POOL] Submitted share not equal to previous block hash\n");
					response = -1;
					valid = false;
				}
				// Check to make sure the submitted block meets all the requirements for block submission
				for(unsigned int i = 0;i < submitted.size();i++) {
					if(submitted[i].nBirthdayA == block.nBirthdayA && submitted[i].nBirthdayB == block.nBirthdayB && submitted[i].nNonce == submitted[i].nNonce) {
						valid = false;
						break;
					} 
				}
			}
			
			
			// TODO: Check PoW here differently
			
			if(valid && block.nVersion == kbp->block.nVersion && block.nBits == kbp->block.nBits && block.hashMerkleRoot == kbp->block.hashMerkleRoot) {
				// Get the length and wheather or not it meets difficulty
				bool b = CheckProofOfWork(block.GetHash(), BlockPayment::SHARE_TARGET);
				if(b) {
					//int length = block.nPrimeChainLength >> 24; // 24 = FRACTIONAL_BITS
					int length = 6; // 6-chain = 1, and that is all we take with this pool
					//std::cout << "Block Submitted: " << length << std::endl;
					if(CheckProofOfWork(block.GetHash(), kbp->block.nBits)) { // BLOCK FOUND!
						CBlockIndex *pindexPrev = pindexBest;
						// Create a copy and send it in
						CBlock *submit = new CBlock();
						submit->nVersion = block.nVersion;
						submit->hashPrevBlock = block.hashPrevBlock;
						submit->hashMerkleRoot = block.hashMerkleRoot;
						submit->nTime = block.nTime;
						submit->nBits = block.nBits;
						submit->nNonce = block.nNonce;
						//submit->bnPrimeChainMultiplier = block.bnPrimeChainMultiplier;
						submit->nBirthdayA = block.nBirthdayA;
						submit->nBirthdayB = block.nBirthdayB;
						submit->vtx = kbp->block.vtx;
						if(CheckWork(submit, *wallet, keys[0].key)) {
							printf("[POOL] Mined block %i\n", pindexPrev->nHeight);
							// Block has been mined, write shares
							//poolQueueLock.lock();
							{
							boost::unique_lock<boost::shared_mutex> lock(poolQueueLock);
							poolQueue.setHeight(pindexBest->nHeight);
							poolQueue.setXPM((double)submit->vtx[0].vout[0].nValue / (double)100000000);
							// Add their big share
							poolQueue.addShares(address, length);
							// Save and begin a new block
							poolQueue.save(std::string(BLOCKS_COMPUTED) + helper::uintToString((unsigned int)pindexBest->nHeight));
							poolQueue.newBlock(0);
							poolQueue.updateUnconfirmedUserBalances();
							}
							//poolQueueLock.unlock();
							saveCurrentBlock();
							response = 1;
						}
					} else {
						// Add to submitted before we pay
						submitted.push_back(block);
						// Respond with difficulty
						if(response != 1) response = length;
						// Pay for share
						//poolQueueLock.lock();
						{
							boost::unique_lock<boost::shared_mutex> lock(poolQueueLock);
							poolQueue.addShares(address, length);
						}
						//poolQueueLock.unlock();
					}
				} else printf("[POOL] Client PoW not met!\n");
			} else printf("[POOL] Client share not valid!\n");
			//if(!xpt) {
				unsigned char *data = new unsigned char[5];
				int code = 1;
				memcpy(data, &code, 1);
				memcpy(data + 1, &response, 4);
				boost::asio::write(*sock, boost::asio::buffer(data, 5));
				delete[] data;
			/*} else {
				xptPacketbuffer_t* p = xptPacketbuffer_create(64 * 1024);
				bool error = false;
				xptPacketbuffer_beginWritePacket(p, XPT_OPC_S_SHARE_ACK);
				
				xptPacketbuffer_writeU32(p, &error, response > 0 ? 0 : 1);
				std::string reason = "Submitted";
				if(response == 0) reason = "Rejected";
				else if(response < 0) reason = "Stale";
				
				xptPacketbuffer_writeString(p, (char *)reason.c_str(), 512, &error);
				xptPacketbuffer_writeFloat(p, &error, response == 1 ? BlockPayment::CHAIN_VALUES[4] : BlockPayment::CHAIN_VALUES[response - 6]);
				xptPacketbuffer_finalizeWritePacket(p);
				//std::cout << "Writing response" << std::endl;
				boost::asio::write(*sock, boost::asio::buffer(p->buffer, p->parserIndex));
				//std::cout << "Response written" << std::endl;
				xptPacketbuffer_free(p);
			}*/
		}
		else {
			printf("[POOL] Client disconnected");
			//clientsLock.lock();
			{
				boost::unique_lock<boost::shared_mutex> lock(clientsLock);
				for(int i = clients.size() - 1;i >= 0;i--) {
					if(clients[i] == this) {
						clients.erase(clients.begin() + i);
						break;
					}
				}
			}
			//clientsLock.unlock();
			delete this;
			return;
		}
		boost::asio::async_read(*sock, boost::asio::buffer(buf, 88),boost::bind(&CPoolClient::HandleSubmission, this,boost::asio::placeholders::error));
	} catch(boost::system::system_error& e) {
		printf("CLIENT EXCEPTION: %s\n", e.what());
	} catch(std::exception& e) {
		printf("STD EXCEPTION: %s\n", e.what());
	}
}

void PoolAcceptThread(boost::asio::io_service* io_service, short port, CWallet* pwallet, bool xpt) {
	tcp::acceptor a(*io_service, tcp::endpoint(tcp::v4(), port));
	for (;;)
	{
		// stop accepting connections after 800
		if(CPoolClient::clients.size() > 600) {
			boost::this_thread::sleep_for(boost::chrono::seconds(1));
			continue;
		}
		boost::shared_ptr<tcp::socket> sock(new tcp::socket(*io_service));
		a.accept(*sock);
		printf("[POOL] Client connection accepted\n");
		CPoolClient *c = new CPoolClient(pwallet, xpt);
		boost::thread t(boost::bind(&CPoolClient::Run, c, sock));
	}
}

void PoolWorkThread(CWallet *pwallet) {
	////int64 nStart = GetTime();
	CBlockIndex *pindexPrev = pindexBest;
	//unsigned int nTransactionsUpdatedLast = nTransactionsUpdated;
	try {
		for(;;) {
			if (/*(nTransactionsUpdated != nTransactionsUpdatedLast && GetTime() - nStart > 45) ||*/(pindexPrev != pindexBest)) {
				// Send new work to clients
				//CPoolClient::clientsLock.lock();
				{
					boost::unique_lock<boost::shared_mutex> lock(CPoolClient::clientsLock);
					for(unsigned int i = 0;i < CPoolClient::clients.size();i++) CPoolClient::clients[i]->Sendwork();
				}
				//CPoolClient::clientsLock.unlock();
				//nStart = GetTime();
				pindexPrev = pindexBest;
				//nTransactionsUpdatedLast = nTransactionsUpdated;
				processBalances(pindexPrev->nHeight, pwallet);
			}
			MilliSleep(500);
		}
	} catch(boost::system::system_error& e) {
		std::cout << "BOOST EXCEPTION: " << e.what() << std::endl;
	} catch(std::exception& e) {
		std::cout << "STD EXCEPTION: " << e.what() << std::endl;
	}
}
	
void PoolSaveThread() {
	int prevShares = poolQueue.getCurrent()->getRunningTotal();
	for(;;) {
		// save current block
		saveCurrentBlock();
		MilliSleep(60000);
		// Record the sharerate
		int shares = poolQueue.getCurrent()->getRunningTotal();
		sharerate = prevShares >= shares ? sharerate : shares - prevShares;
		prevShares = shares;
	}
}

void StartPool(boost::thread_group& threadGroup, CWallet* pwallet) {
	std::cout << "Starting pool" << std::endl;
	// Load queue
	
	namespace fs = boost::filesystem;
	fs::path someDir(GetDataDir());
	someDir += BLOCKS_LOCATION;
	std::string path = someDir.string();
	printf("[POOL] Blocks Path: %s\n", path.c_str());
	BLOCKS_COMPUTED = helper::stringToCstring(path);
	std::vector <std::string> result;
	dirent* de;
	DIR* dp;
	errno = 0;
	dp = opendir( path.empty() ? "." : path.c_str() );
	if (dp)
	{
		while (true)
		{
			errno = 0;
			de = readdir( dp );
			if (de == NULL) break;
			std::string name = de->d_name;
			if(name == "." || name == "..") continue;
			result.push_back( std::string( de->d_name ) );
		}
		closedir( dp );
		std::sort( result.begin(), result.end() );
	}
	//std::cout << "Loading " << result.size() << " blocks..." << std::endl;
	for(unsigned int i = 0;i < result.size();i++) poolQueue.loadNewBlock(path + result[i]);
	//std::cout << "Loaded." << std::endl;
	bool current = false;
	if(poolQueue.getCurrent() != NULL) current = true;
	// Get current and if is not = to block height
	if(!current) poolQueue.newBlock(0);
	poolQueue.updateUnconfirmedUserBalances();

	loadUserInfos();
	boost::asio::io_service* io = getIO_service();

	// Pause 3 seconds, then start
	boost::this_thread::sleep_for(boost::chrono::seconds(8));

	threadGroup.create_thread(boost::bind(&PoolAcceptThread, io, 1339, pwallet, false)); // Accept xolokram clients
	//threadGroup.create_thread(boost::bind(&PoolAcceptThread, io, 10034, pwallet, true)); // Accept xpt clients
	threadGroup.create_thread(boost::bind(&PoolWorkThread, pwallet));
	threadGroup.create_thread(boost::bind(&PoolSaveThread));
	
}
