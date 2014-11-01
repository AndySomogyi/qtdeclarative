/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef QV4TYPEDARRAY_H
#define QV4TYPEDARRAY_H

#include "qv4object_p.h"
#include "qv4functionobject_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct ArrayBuffer;

typedef ReturnedValue (*TypedArrayRead)(const char *data, int index);
typedef void (*TypedArrayWrite)(ExecutionEngine *engine, char *data, int index, ValueRef value);

struct TypedArrayOperations {
    int bytesPerElement;
    const char *name;
    TypedArrayRead read;
    TypedArrayWrite write;
};

struct TypedArray : Object
{
    enum Type {
        Int8Array,
        UInt8Array,
        UInt8ClampedArray,
        Int16Array,
        UInt16Array,
        Int32Array,
        UInt32Array,
        Float32Array,
        Float64Array,
        NTypes
    };

    struct Data : Object::Data {
        Data(ExecutionEngine *e, Type t);

        const TypedArrayOperations *type;
        ArrayBuffer *buffer;
        uint byteLength;
        uint byteOffset;
    };
    V4_OBJECT(Object)

    uint length() const {
        return d()->byteLength/d()->type->bytesPerElement;
    }


    static void markObjects(HeapObject *that, ExecutionEngine *e);
    static ReturnedValue getIndexed(Managed *m, uint index, bool *hasProperty);
    static void putIndexed(Managed *m, uint index, const ValueRef value);
};

struct TypedArrayCtor: FunctionObject
{
    struct Data : FunctionObject::Data {
        Data(ExecutionContext *scope, TypedArray::Type t);

        TypedArray::Type type;
    };

    V4_OBJECT(FunctionObject)

    static ReturnedValue construct(Managed *m, CallData *callData);
    static ReturnedValue call(Managed *that, CallData *callData);
};


struct TypedArrayPrototype : Object
{
    struct Data : Object::Data {
        Data(ExecutionEngine *e, TypedArray::Type t)
            : Object::Data(e)
            , type(t)
        {
            setVTable(staticVTable());
        }
        TypedArray::Type type;
    };
    V4_OBJECT(Object)

    void init(ExecutionEngine *engine, TypedArrayCtor *ctor);

    static ReturnedValue method_get_buffer(CallContext *ctx);
    static ReturnedValue method_get_byteLength(CallContext *ctx);
    static ReturnedValue method_get_byteOffset(CallContext *ctx);
    static ReturnedValue method_get_length(CallContext *ctx);

    static ReturnedValue method_set(CallContext *ctx);
    static ReturnedValue method_subarray(CallContext *ctx);
};

} // namespace QV4

QT_END_NAMESPACE

#endif
