#!/usr/bin/python

# This file contains debug dumpers / helpers / visualizers so that certain voglcore
# classes can be more easily inspected by gdb and QtCreator.

def qdump__vogl__dynamic_string(d, value):
    dyn = value["m_dyn"]
    small = value["m_small"]
    len = value["m_len"]
    small_flag = small["m_flag"]
    d.putAddress(value.address)
    buf = dyn["m_pStr"]
    if small_flag == 1:
        buf = small["m_buf"]
    strPrefix = "[%d] " % int(len)
    str = "'" + buf.string(length=len) + "'"
    d.putValue(strPrefix + str)
    d.putNumChild(3)
    with Children(d):
        d.putSubItem("m_len", len)
        with SubItem(d, "m_small"):
            d.putValue( str if small_flag == 1 else "<ignored>")
            d.putNumChild(2)
            with Children(d):
                d.putSubItem("m_flag", small_flag)
                with SubItem(d, "m_buf"):
                    d.putValue(str if small_flag == 1 else "<ignored>")
        with SubItem(d, "m_dyn"):
            d.putValue("<ignored>" if small_flag == 1 else str)
            d.putNumChild(2)
            with Children(d):
                with SubItem(d, "m_buf_size"):
                    d.putValue("<ignored>" if small_flag == 1 else dyn["m_buf_size"])
                with SubItem(d, "m_pStr"):
                    d.putValue("<ignored>" if small_flag == 1 else str)

def qdump__vogl__vector(d, value):
    size = value["m_size"]
    capacity = value["m_capacity"]
    data = value["m_p"]
    maxDisplayItems = 100
    innerType = d.templateArgument(value.type, 0)
    p = data
    d.putAddress(value.address)
    d.putValue("[%d]" % size)
    d.putNumChild(3)
    if d.isExpanded():
        with Children(d):
            d.putSubItem("m_capacity", capacity)
            d.putSubItem("m_size", size)
            with SubItem(d, "m_p"):
                d.putItemCount(size)
                d.putNumChild(size)
                numDisplayItems = min(maxDisplayItems, size)
                with Children(d, size, maxNumChild=numDisplayItems, childType=innerType, addrBase=p, addrStep=p.dereference().__sizeof__):
                    for i in range(0,numDisplayItems):
                        d.putSubItem(i, p.dereference())
                        p += 1


