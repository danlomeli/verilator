// $Id$ //-*- C++ -*-
//*************************************************************************
// DESCRIPTION: Verilator: Propagate constants across AST
//
// Code available from: http://www.veripool.com/verilator
//
// AUTHORS: Wilson Snyder with Paul Wasson, Duane Gabli
//
//*************************************************************************
//
// Copyright 2003-2006 by Wilson Snyder.  This program is free software; you can
// redistribute it and/or modify it under the terms of either the GNU
// General Public License or the Perl Artistic License.
//
// Verilator is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//*************************************************************************

#ifndef _V3CONST_H_
#define _V3CONST_H_ 1
#include "config.h"
#include "V3Error.h"
#include "V3Ast.h"

//============================================================================

class V3Const {
public:
    // Force this cell node's parameter list to become a constant
    static void constifyParam(AstNode* nodep);
    // Everything that's possible
    static void constifyAll(AstNetlist* nodep);
    // Also, warn
    static void constifyAllLint(AstNetlist* nodep);
    // C++ datatypes
    static void constifyCpp(AstNetlist* nodep);
    // Only the current node and lower
    static void constifyTree(AstNode* nodep);
};

#endif // Guard
