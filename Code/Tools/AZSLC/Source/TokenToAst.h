/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

#pragma once

#include "Antlr4.h"

#include <unordered_map>

namespace AZ::ShaderCompiler
{
    /// Observation: A CST's leaves are "token object" pointers.
    ///  But there is no way to find the reverse association: from token to AST.
    /// This class fills that gap.
    class TokenToAst
    {
    public:
        using AstNode = antlr4::ParserRuleContext;

        AstNode* GetNode(const ssize_t tokenId)
        {
            const auto iterator = m_tokenToAst.find(tokenId);
            if (iterator == m_tokenToAst.end())
            {
                return nullptr;
            }

            return iterator->second;
        }

        void SetAssociation(const ssize_t tokenId, AstNode* node)
        {
            m_tokenToAst[tokenId] = node;
        }

    protected:
        // generic tokenid to ast pointer map
        std::unordered_map<ssize_t, AstNode*> m_tokenToAst;
    };
}
