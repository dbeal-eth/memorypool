#!/usr/bin/env python
# -*- coding: utf-8 -*-

from string import Template
from datetime import date
import os

# mmc db dir
mmc_db="/Users/kral/Library/Application\ Support/MemoryCoin/";
# relase dir
relase_dir="../../../build-memorycoin-qt-Desktop_Qt_5_2_0_clang_64bit-Release/"


# Open MMc info.plist
File    = relase_dir+"MemoryCoin-Qt.app/Contents/Info.plist"
version    = "unknown";
# Read Qt config File grab version
fileForGrabbingVersion = "../../memorycoin-qt.pro"
for line in open(fileForGrabbingVersion):
        lineArr = line.replace(" ", "").split("=");
        if lineArr[0].startswith("VERSION"):
                version = lineArr[1].replace("\n", "");

fIn = open(File, "r")
fileContent = fIn.read()
s = Template(fileContent)
#Change VERSION and YEAR
newFileContent = s.substitute(VERSION=version,YEAR=date.today().year)
# Save new config
fo = open(File, "w")
fo.write(newFileContent);

print "Info.plist reload"


# Fast Sync
os.system("cp "+mmc_db+"hashlookup.txt "+relase_dir+"MemoryCoin-Qt.app/Contents/MacOS/trustedhashlookup.txt")
print "trustedhashlookup.txt reload"

# TODO: add yam miner
# copy yam miner
#os.system('cp ./yam "+relase_dir+"MemoryCoin-Qt.app/Resources/yam')
# chmod
#os.system('chmod +x "+relase_dir+"MemoryCoin-Qt.app/Resources/yam')
