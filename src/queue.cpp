#include "blockpayment.h"

using namespace BlockPayment;

Queue::Queue() {
    h = NULL;
    r = NULL;
    unconfirusrbals.clear();
}

Queue::~Queue() {
    r = NULL;
    delete h;
}

Nodes::Block* Queue::getCurrent() {
    return r;
}

Nodes::Block* Queue::peek() {
    return h;
}

Nodes::Block* Queue::deq() {
    if(h == NULL) return NULL;
    Nodes::Block* t = h;
    h = h->getNext();
    if(h == NULL) r = NULL; //if t == r
    t->setNext(NULL); //makes it so when deleted it does not delete the whole list
    return t;
}

void Queue::newBlock(unsigned int newheight) {
    if(h == NULL) {
        h = new Nodes::Block(newheight);
        r = h;
    }
    else {
        if(r == NULL) { //not sure how but fix
            Nodes::Block* i = h; //iterator
            while(i->getNext() != NULL) i = i->getNext();
            r = i;
        }
        r->setNext(new Nodes::Block(newheight));
        r = r->getNext();
    }
}

void Queue::addShares(std::string user, unsigned int chainlength) {
    if(r == NULL) {
        if(h == NULL) { //nothing off
            return; // a chunk needs to be added
        }
        else { //not sure how but fix
            Nodes::Block* i = h; //iterator
            while(i->getNext() != NULL) i = i->getNext();
            r = i;
            r->addShares(user, chainlength);
        }
    }
    else {
        r->addShares(user, chainlength);
    }
}

void Queue::setHeight(unsigned int in, bool current) {
    if(current) {
        if(r == NULL) return;
        r->setHeight(in);
    }
    else {
        if(h == NULL) return;
        h->setHeight(in);
    }
}

void Queue::setXPM(double in, bool current) {
    if(current) {
        if(r == NULL) return;
        r->setXPM(in);
    }
    else {
        if(h == NULL) return;
        h->setXPM(in);
    }
}

void Queue::calculateXPMPerUser(bool current) {
    if(current) {
        if(r == NULL) return;
        r->calculateXPMPerUser();
    }
    else {
        if(h == NULL) return;
        h->calculateXPMPerUser();
    }
}

void Queue::calculateXPMPerUser(double XPM, bool current) {
    if(current) {
        if(r == NULL) return;
        r->calculateXPMPerUser(XPM);
    }
    else {
        if(h == NULL) return;
        h->calculateXPMPerUser(XPM);
    }
}

void Queue::calculateAllXPM() {
    Nodes::Block* i = h;
    while(i != NULL) {
        if((!i->hasCalculatedXPM()) && (i->getXPM() != 0)) i->calculateXPMPerUser();
        i = i->getNext();
    }
}

void Queue::updateUnconfirmedUserBalances() {
    calculateAllXPM();
    unconfirusrbals.clear();
    Nodes::Block* i1 = h;
    while(i1 != NULL) {
        Nodes::User* i2 = i1->getUserList();
        if (i1->hasCalculatedXPM()) {
            while(i2 != NULL) {
                if(unconfirusrbals.count(i2->username) > 0) unconfirusrbals[i2->username] += i2->XPM;
                else unconfirusrbals[i2->username] = i2->XPM;
                i2 = i2->next;
            }
        }
        i1 = i1->getNext();
    }
}

double Queue::getUsersUnconfirmedBalance(std::string user, bool update) {
    if(update) updateUnconfirmedUserBalances();
    if(unconfirusrbals.count(user) > 0) return unconfirusrbals[user];
    return 0;
}

unsigned int Queue::getHeight(bool current) {
    if(current) {
        if(r == NULL) return 0;
        else return r->getHeight();
    }
    else {
        if(h == NULL) return 0;
        else return h->getHeight();
    }
}

double Queue::getXPM(bool current) {
    if(current) {
        if(r == NULL) return 0;
        else return r->getXPM();
    }
    else {
        if(h == NULL) return 0;
        else return h->getXPM();
    }
}

Nodes::User* Queue::getUserList(bool current) {
    if(current) {
        if(r == NULL) return NULL;
        else return r->getUserList();
    }
    else {
        if(h == NULL) return NULL;
        else return h->getUserList();
    }
}

bool Queue::hasCalculatedXPM(bool current) {
    if(current) {
        if(r == NULL) return false;
        return r->hasCalculatedXPM();
    }
    else {
        if(h == NULL) return false;
        return h->hasCalculatedXPM();
    }
}

void Queue::save(std::string file, bool current) {
    if(current) {
        if(r == NULL) return;
        r->save(file);
    }
    else {
        if(h == NULL) return;
        h->save(file);
    }
}

void Queue::load(std::string file) {
    Nodes::Block* t = Nodes::Block::load(file);
    if(t == NULL) return; //do not change anything if it could not find the file
    if(r != NULL) {
        delete r;
    }
    r = t;
    if(h == NULL) h = r;
}

void Queue::loadNewBlock(std::string file) {
    if(r == NULL) { //if r is null h must be null
        r = Nodes::Block::load(file);
        h = r;
    }
    else {
        r->setNext(Nodes::Block::load(file));
        if(r->getNext() != NULL) r = r->getNext();
    }
}

void Queue::loadNewBlocks(std::vector<std::string> files) {
    for(unsigned int x = 0; x < files.size(); x++) loadNewBlock(files[x]);
}
