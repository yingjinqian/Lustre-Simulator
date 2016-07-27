TEMPLATE = app
CONFIG += console c++11

CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    blockdevice.cpp \
    client.cpp \
    cluster.cpp \
    congestcontroller.cpp \
    device.cpp \
    disk.cpp \
    elevator.cpp \
    elvdeadline.cpp \
    elvnoop.cpp \
    event.cpp \
    filesystem.cpp \
    intervaltree.cpp \
    ldlm.cpp \
    lov.cpp \
    lustre.cpp \
    mdc.cpp \
    mdt.cpp \
    message.cpp \
    netdevice.cpp \
    network.cpp \
    nic.cpp \
    node.cpp \
    nrsbinheap.cpp \
    nrsepoch.cpp \
    nrsfifo.cpp \
    nrsfrr.cpp \
    nrsprio.cpp \
    nrsrbtree.cpp \
    obd.cpp \
    osc.cpp \
    osd.cpp \
    ost.cpp \
    peer.cpp \
    pios.cpp \
    processor.cpp \
    ptlrpc.cpp \
    raid0device.cpp \
    rbtree.cpp \
    scheduler.cpp \
    server.cpp \
    stat.cpp \
    timer.cpp \
    heap.cpp \
    cfshash.cpp \
    nrstbf.cpp

DISTFILES += \
    lustre.pro.user

HEADERS += \
    blockdevice.h \
    client.h \
    cluster.h \
    congestcontroller.h \
    device.h \
    disk.h \
    elevator.h \
    elvdeadline.h \
    elvnoop.h \
    event.h \
    filesystem.h \
    intervaltree.h \
    ldlm.h \
    lov.h \
    lustre.h \
    mdc.h \
    mdt.h \
    message.h \
    netdevice.h \
    network.h \
    nic.h \
    node.h \
    nrsbinheap.h \
    nrsepoch.h \
    nrsfifo.h \
    nrsfrr.h \
    nrsprio.h \
    nrsrbtree.h \
    obd.h \
    osc.h \
    osd.h \
    ost.h \
    peer.h \
    pios.h \
    processor.h \
    ptlrpc.h \
    raid0device.h \
    rbtree.h \
    scheduler.h \
    server.h \
    stat.h \
    timer.h \
    heap.h \
    hash.h \
    cfshash.h \
    nrstbf.h
