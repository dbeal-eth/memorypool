#!/usr/bin/env python
# -*- coding: utf-8 -*-

from string import Template
from datetime import date
import os

# mmc db dir
mmc_db="/Users/kral/Library/Application\ Support/MemoryCoin/";
# relase dir
relase_dir="../../../build-memorycoin-qt-Desktop_Qt_5_2_0_clang_64bit-Release/"
#Mac Deploy Tool for dep.
mac_dep="/Users/kral/Qt5.2.0/5.2.0/clang_64/bin/macdeployqt"


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

# copy yam miner
os.system('cp ./yam '+relase_dir+'MemoryCoin-Qt.app/Contents/MacOS/yam')
# chmod
os.system('chmod +x '+relase_dir+'MemoryCoin-Qt.app/Contents/MacOS/yam')
print "Yam Miner Copied"
os.system('cp ./yam-aesni-off.cfg '+relase_dir+'MemoryCoin-Qt.app/Contents/MacOS/yam-aesni-off.cfg')
os.system('cp ./yam-aesni-on.cfg '+relase_dir+'MemoryCoin-Qt.app/Contents/MacOS/yam-aesni-on.cfg')
print "Yam Miner Configs Copied"

# Mac Deploy
#os.system(mac_dep+' '+relase_dir+'MemoryCoin-Qt.app -verbose=3')



#otool -l ../../../build-memorycoin-qt-Desktop_Qt_5_2_0_clang_64bit-Release/MemoryCoin-Qt.app/Contents/MacOS/MemoryCoin-Qt

#/Users/kral/Qt5.2.0/5.2.0/clang_64/bin/macdeployqt ../../../build-memorycoin-qt-Desktop_Qt_5_2_0_clang_64bit-Release/MemoryCoin-Qt.app
