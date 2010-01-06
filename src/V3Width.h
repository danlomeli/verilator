// -*- C++ -*-
//*************************************************************************
// DESCRIPTION: Verilator: Node attributes/ expression widths
//
// Code available from: http://www.veripool.org/verilator
//
// AUTHORS: Wilson Snyder with Paul Wasson, Duane Gabli
//
//*************************************************************************
//
// Copyright 2003-2010 by Wilson Snyder.  This program is free software; you can
// redistribute it and/or modify it under the terms of either the GNU
// Lesser General Public License Version 3 or the Perl Artistic License
// Version 2.0.
//
// Verilator is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//*************************************************************************

#ifndef _V3WIDTH_H_
#define _V3WIDTH_H_ 1
#include "config_build.h"
#include "verilatedos.h"
#include "V3Error.h"
#include "V3Ast.h"

//============================================================================

class V3Width {
public:
    static void width(AstNetlist* nodep);
    // Smaller step... Only do a single node for parameter propagation
    static AstNode* widthParamsEdit(AstNode* nodep);
    static AstNode* widthParamsEditIfNeed(AstNode* nodep);
    // Final step... Mark all widths as equal
    static void widthCommit(AstNetlist* nodep);

    // For use only in WidthVisitor
    // Replace AstSelBit, etc with AstSel/AstArraySel
    // Returns replacement node if nodep was deleted, or null if none.
    static AstNode* widthSelNoIterEdit(AstNode* nodep);
};

#endif // Guard
