#include "blockpayment.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_print.hpp"
#include "helper.h"

using namespace BlockPayment::Nodes;

Block::Block(unsigned int newheight) {
    next = NULL;
    h = NULL;
    height = newheight;
    runningtotal = 0;
    XPM = 0;
    calculatedXPM = false;
}

Block::~Block() {
    if(h != NULL) delete h;
    if(next != NULL) delete next;
}

std::vector<std::string> Block::getUserIdList() {
    std::vector<std::string> useridlist;
    if(h == NULL) return useridlist;
    User* i = h; //iterator
    do {
        useridlist.push_back(i->username);
        i = i->next;
    } while (i != NULL);
    return useridlist;
}

User* Block::getUser(std::string user) { //do not use to many times, it has to start at the begining each time
    if(h == NULL) return NULL;
    User* i = h; //iterator
    while((user > i->username) && (i->next != NULL)) i = i->next;
    if(user == i->username) return i;
    return NULL;
}

void Block::addShares(std::string user, unsigned int chainlength) {
    if(chainlength < MIN_CHAIN_LENGTH) return; //it does not need to do anything
    calculatedXPM = false;
    unsigned int shares = calcShares(chainlength);
    runningtotal += shares;
    if(h == NULL) {
        h = new User(user, shares);
        return; //keeps there from being too many if statement scopes, becomes hard to read
    }

    User* i = h; //iterator
    if(user < i->username) {
        User* t = i;
        i = new User(user, shares);
        i->next = t;
        h = i;
        return;
    }
    else if(user == i->username) {
        i->runningtotal += shares;
        return;
    }
    else if(i->next == NULL) {
        i->next = new User(user, shares);
        return;
    }
    while((user > i->next->username) && (i->next->next != NULL)) {
        i = i->next;
    }
    if(user < i->next->username) {
        User* t = i->next;
        i->next = new User(user, shares);
        i->next->next = t;
    }
    else if(user == i->next->username) {
        i->next->runningtotal += shares;
    }
    else {
        i->next->next = new User(user, shares);
    }
}

void Block::calculateXPMPerUser() {
    if(h == NULL) return;
    User* i = h; //iterator
    do {
        i->XPM = (XPM * ((double)i->runningtotal / (double)runningtotal) * (1 - FEE));
        i = i->next;
    } while (i != NULL);
    calculatedXPM = true;
}

void Block::calculateXPMPerUser(double newxpm) {
    XPM = newxpm;
    calculateXPMPerUser();
}

unsigned int Block::getHeight() {
    return height;
}

double Block::getXPM() {
    return XPM;
}

unsigned int Block::getRunningTotal() {
    return runningtotal;
}

User* Block::getUserList() {
    return h;
}

Block* Block::getNext() {
    return next;
}

bool Block::hasCalculatedXPM() {
    return calculatedXPM;
}

void Block::deleteNext() {
    if(next != NULL) delete next;
}

void Block::setNext(Block* newnext) {
    next = newnext;
}

void Block::setUser(User* in) {
    if(h != NULL) delete h;
    h = in;
}

void Block::setHeight(unsigned int in) {
    height = in;
}

void Block::setXPM(double newxpm) {
    calculatedXPM = false;
    XPM = newxpm;
}

void Block::setRunningTotal(unsigned int total) {
    runningtotal = total;
}

void Block::save(std::string file) {
    unsigned int x = 0; //current place in cstrings
    std::vector<char*> cstrings; //this is redicilous
    rapidxml::xml_document<> doc;
    rapidxml::xml_node<>* node = doc.allocate_node(rapidxml::node_element, "block");

    cstrings.push_back(helper::stringToCstring(helper::uintToString(height)));
    node->append_attribute(doc.allocate_attribute("height", cstrings[x])); x++;

    cstrings.push_back(helper::stringToCstring(helper::doubleToString(XPM)));
    node->append_attribute(doc.allocate_attribute("xpm", cstrings[x])); x++;

    cstrings.push_back(helper::stringToCstring(helper::uintToString(runningtotal)));
    node->append_attribute(doc.allocate_attribute("total_shares", cstrings[x])); x++;

    if(h == NULL) return; //nothing more to save
    User* i = h; //iterator
    do {
        rapidxml::xml_node<> *usernode = doc.allocate_node(rapidxml::node_element, "user");

        cstrings.push_back(helper::stringToCstring(i->username));
        usernode->append_attribute(doc.allocate_attribute("name", cstrings[x])); x++;

        cstrings.push_back(helper::stringToCstring(helper::uintToString(i->runningtotal)));
        usernode->append_attribute(doc.allocate_attribute("shares", cstrings[x])); x++;

        node->append_node(usernode);
        i = i->next;
    } while (i != NULL);

    doc.append_node(node);

    std::ofstream out(file.c_str());
    out << doc;
    out.close();
    doc.clear();
    for(unsigned int x = 0; x < cstrings.size(); x++) delete[] cstrings[x];
}

Block* Block::load(std::string file) { //static
    //thanks http://semidtor.wordpress.com/2012/12/15/readwrite-xml-file-with-rapidxml/
    std::ifstream in(file.c_str());
    if(!in) return NULL; //file does not exist
    rapidxml::xml_document<> doc;
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string content(buffer.str());
    doc.parse<0>(&content[0]);
    rapidxml::xml_node<>* node = doc.first_node("block");
    Block* out = new Block(helper::stringToUInt(node->first_attribute("height")->value()));
    out->setXPM(helper::stringToDouble(node->first_attribute("xpm")->value()));
    out->setRunningTotal(helper::stringToUInt(node->first_attribute("total_shares")->value()));
    User* i = NULL;
    rapidxml::xml_node<>* users = node->first_node("user");
    while(users != NULL) {
        std::string name = users->first_attribute("name")->value();
        unsigned int shares = helper::stringToUInt(users->first_attribute("shares")->value());
        if(i == NULL) {
            i = new User(name, shares);
            out->setUser(i);
        }
        else {
            i->next = new User(name, shares);
            i = i->next;
        }
        users = users->next_sibling("user");
    }
    in.close();
    doc.clear();
    return out;
}



unsigned int Block::calcShares(unsigned int chainlength) {
    if(chainlength < MIN_CHAIN_LENGTH) return 0;

    return (chainlength >= MAX_CHAIN_LENGTH) ?
    CHAIN_VALUES[MAX_CHAIN_LENGTH - MIN_CHAIN_LENGTH] : //the largest in array
    CHAIN_VALUES[chainlength - MIN_CHAIN_LENGTH];
}
