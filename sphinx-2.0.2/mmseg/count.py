# -*- coding:utf-8 -*-
__author__ = 'xdpan'

import os
import MySQLdb
import MySQLdb.cursors


DB_HOST = "127.0.0.1"
DB_USER = "root"
DB_PASS = "root!@#"
DB_NAME = "hy_web"

G_DB = None
G_DEFAULT_DICT = "/usr/local/servers/mmseg-3.2.14/etc/unigram.txt"
G_OUT_DICT = "/tmp/mydict.txt"
#G_TABLE = "hy_messages_ext3"


def openDB():
    db = MySQLdb.connect(DB_HOST, DB_USER, DB_PASS, DB_NAME, cursorclass=MySQLdb.cursors.DictCursor)
    conn = db.cursor()
    conn.execute("SET NAMES utf8")
    return db


def closeDB(db):
    if db:
        db.close()


def selectLine(sql, db):
    cursor = db.cursor()
    cursor.execute(sql)
    return cursor.fetchone()


def selectLines(sql, db):
    cursor = db.cursor()
    cursor.execute(sql)
    return cursor.fetchall()


def tokenWord(s):
    filename = "/tmp/doc.8888.txt"
    fd = open(filename, "w")
    fd.write(s)
    fd.close()
    cmd = "/usr/local/servers/mmseg-3.2.14/bin/mmseg -d /usr/local/servers/mmseg-3.2.14/etc %s|grep -v 'Word Splite'" % filename
    data = os.popen(cmd).read()
    lst = data.split()
    return lst


def getDoc(id, table, db):
    sql = "select * from %s where mid=%d" % (table, id)
    ret = selectLine(sql, db)
    if ret:
        txt = ret["content"]
    return txt


def getDocs(start, end, table, db):
    sql = "select * from %s where mid>%d and mid<=%s" % (table, start, end)
    rets = selectLines(sql, db)
    lst = []
    for e in rets:
        txt = e["content"]
        lst.append(txt)
    return " ".join(lst)


def getMax(table, db):
    sql = "select max(mid) as max from %s" % table
    return selectLine(sql, db)


def getMin(table, db):
    sql = "select min(mid) as min from %s" % table
    return selectLine(sql, db)


def saveDct(dct):
    fd = open(G_OUT_DICT, "w+")
    for k, v in dct.items():
        fd.write("%s\t%s\nx:%s\n" % (k, v, v))
    fd.close()


def loadDct():
    dct = {}

    filename = G_OUT_DICT
    if not os.path.exists(filename):
        filename = G_DEFAULT_DICT
    fd = open(filename)
    while True:
        line = fd.readline()
        if not line:
            break
        if line.startswith("x:"):
            continue
        (k, v) = line.split()
        k = k.strip()
        v = v.strip()
        dct[k] = int(v)
    return dct

def main(table):
    G_DB = openDB()
    max = 0
    min = 0
    ret = getMax(table, G_DB)
    if ret:
        max = ret["max"]
    ret = getMin(table, G_DB)
    if ret:
        min = ret["min"]
    idx = min
    dct = loadDct()
    step = 10000
    while idx <= max:
        print "start idx: %d " % idx
        txt = getDocs(idx, idx + step, table, G_DB)
        lst = tokenWord(txt)
        for e in lst:
            e = e.strip().strip("/x")
            if e.isdigit() or e.isalpha() or e.isspace():
                continue
            if e:
                if dct.has_key(e):
                    dct[e] += 1
        idx += step
    saveDct(dct)
    closeDB(G_DB)



if __name__ == "__main__":
    for table in ["hy_messages_ext4", "hy_messages_ext5", "hy_messages_ext6", "hy_messages_ext7", "hy_messages_ext8", "hy_messages_ext9"]:
        main(table)
