
// Generated from azslParser.g4 by ANTLR 4.13.2

#pragma once


#include "Antlr4.h"
#include "azslParserVisitor.h"


/**
 * This class provides an empty implementation of azslParserVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  azslParserBaseVisitor : public azslParserVisitor {
public:

  virtual std::any visitCompilationUnit(azslParser::CompilationUnitContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTopLevelDeclaration(azslParser::TopLevelDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIdExpression(azslParser::IdExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnqualifiedId(azslParser::UnqualifiedIdContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitQualifiedId(azslParser::QualifiedIdContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNestedNameSpecifier(azslParser::NestedNameSpecifierContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitClassDefinitionStatement(azslParser::ClassDefinitionStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitClassDefinition(azslParser::ClassDefinitionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBaseList(azslParser::BaseListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitClassMemberDeclaration(azslParser::ClassMemberDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructDefinitionStatement(azslParser::StructDefinitionStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructDefinition(azslParser::StructDefinitionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructMemberDeclaration(azslParser::StructMemberDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAnyStructuredTypeDefinitionStatement(azslParser::AnyStructuredTypeDefinitionStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEnumDefinitionStatement(azslParser::EnumDefinitionStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEnumDefinition(azslParser::EnumDefinitionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnscopedEnum(azslParser::UnscopedEnumContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitScopedEnum(azslParser::ScopedEnumContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEnumeratorListDefinition(azslParser::EnumeratorListDefinitionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEnumeratorDeclarator(azslParser::EnumeratorDeclaratorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAnyStructuredTypeDefinition(azslParser::AnyStructuredTypeDefinitionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInterfaceDefinitionStatement(azslParser::InterfaceDefinitionStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInterfaceDefinition(azslParser::InterfaceDefinitionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInterfaceMemberDeclaration(azslParser::InterfaceMemberDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitConstantBufferTemplated(azslParser::ConstantBufferTemplatedContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitVariableDeclarationStatement(azslParser::VariableDeclarationStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFunctionParams(azslParser::FunctionParamsContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFunctionParam(azslParser::FunctionParamContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitHlslSemantic(azslParser::HlslSemanticContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitHlslSemanticName(azslParser::HlslSemanticNameContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAttributeArguments(azslParser::AttributeArgumentsContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAttributeArgumentList(azslParser::AttributeArgumentListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGlobalAttribute(azslParser::GlobalAttributeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAttachedAttribute(azslParser::AttachedAttributeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAttributeSpecifier(azslParser::AttributeSpecifierContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAttributeSpecifierSequence(azslParser::AttributeSpecifierSequenceContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAttributeSpecifierAny(azslParser::AttributeSpecifierAnyContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlock(azslParser::BlockContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStatement(azslParser::StatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForInitializer(azslParser::ForInitializerContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCaseSwitchLabel(azslParser::CaseSwitchLabelContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDefaultSwitchLabel(azslParser::DefaultSwitchLabelContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSwitchSection(azslParser::SwitchSectionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSwitchBlock(azslParser::SwitchBlockContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEmptyStatement(azslParser::EmptyStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBlockStatement(azslParser::BlockStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExpressionStatement(azslParser::ExpressionStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIfStatement(azslParser::IfStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSwitchStatement(azslParser::SwitchStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitWhileStatement(azslParser::WhileStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDoStatement(azslParser::DoStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForStatement(azslParser::ForStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBreakStatement(azslParser::BreakStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitContinueStatement(azslParser::ContinueStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDiscardStatement(azslParser::DiscardStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitReturnStatement(azslParser::ReturnStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExtenstionStatement(azslParser::ExtenstionStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeAliasingDefinitionStatementLabel(azslParser::TypeAliasingDefinitionStatementLabelContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitElseClause(azslParser::ElseClauseContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParenthesizedExpression(azslParser::ParenthesizedExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMemberAccessExpression(azslParser::MemberAccessExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPrefixUnaryExpression(azslParser::PrefixUnaryExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLiteralExpression(azslParser::LiteralExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitConditionalExpression(azslParser::ConditionalExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPostfixUnaryExpression(azslParser::PostfixUnaryExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNumericConstructorExpression(azslParser::NumericConstructorExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFunctionCallExpression(azslParser::FunctionCallExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIdentifierExpression(azslParser::IdentifierExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBinaryExpression(azslParser::BinaryExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignmentExpression(azslParser::AssignmentExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCastExpression(azslParser::CastExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrayAccessExpression(azslParser::ArrayAccessExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitOtherExpression(azslParser::OtherExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCommaExpression(azslParser::CommaExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPostfixUnaryOperator(azslParser::PostfixUnaryOperatorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPrefixUnaryOperator(azslParser::PrefixUnaryOperatorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBinaryOperator(azslParser::BinaryOperatorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignmentOperator(azslParser::AssignmentOperatorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArgumentList(azslParser::ArgumentListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArguments(azslParser::ArgumentsContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitVariableDeclaration(azslParser::VariableDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitVariableDeclarators(azslParser::VariableDeclaratorsContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUnnamedVariableDeclarator(azslParser::UnnamedVariableDeclaratorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNamedVariableDeclarator(azslParser::NamedVariableDeclaratorContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitVariableInitializer(azslParser::VariableInitializerContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStandardVariableInitializer(azslParser::StandardVariableInitializerContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrayElementInitializers(azslParser::ArrayElementInitializersContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitArrayRankSpecifier(azslParser::ArrayRankSpecifierContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPackOffsetNode(azslParser::PackOffsetNodeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStorageFlags(azslParser::StorageFlagsContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStorageFlag(azslParser::StorageFlagContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitType(azslParser::TypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPredefinedType(azslParser::PredefinedTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSubobjectType(azslParser::SubobjectTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitOtherViewResourceType(azslParser::OtherViewResourceTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRtxBuiltInTypes(azslParser::RtxBuiltInTypesContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBufferPredefinedType(azslParser::BufferPredefinedTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBufferType(azslParser::BufferTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitByteAddressBufferTypes(azslParser::ByteAddressBufferTypesContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPatchPredefinedType(azslParser::PatchPredefinedTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPatchType(azslParser::PatchTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSamplerStatePredefinedType(azslParser::SamplerStatePredefinedTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitScalarType(azslParser::ScalarTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStreamOutputPredefinedType(azslParser::StreamOutputPredefinedTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStreamOutputObjectType(azslParser::StreamOutputObjectTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructuredBufferPredefinedType(azslParser::StructuredBufferPredefinedTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStructuredBufferName(azslParser::StructuredBufferNameContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTextureType(azslParser::TextureTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTexturePredefinedType(azslParser::TexturePredefinedTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericTexturePredefinedType(azslParser::GenericTexturePredefinedTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTextureTypeMS(azslParser::TextureTypeMSContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMsTexturePredefinedType(azslParser::MsTexturePredefinedTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSubpassInputType(azslParser::SubpassInputTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSubpassInputPredefinedType(azslParser::SubpassInputPredefinedTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericSubpassInputPredefinedType(azslParser::GenericSubpassInputPredefinedTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitVectorType(azslParser::VectorTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericVectorType(azslParser::GenericVectorTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitScalarOrVectorType(azslParser::ScalarOrVectorTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitScalarOrVectorOrMatrixType(azslParser::ScalarOrVectorOrMatrixTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMatrixType(azslParser::MatrixTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericMatrixPredefinedType(azslParser::GenericMatrixPredefinedTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRegisterAllocation(azslParser::RegisterAllocationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSamplerStateProperty(azslParser::SamplerStatePropertyContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLiteral(azslParser::LiteralContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLeadingTypeFunctionSignature(azslParser::LeadingTypeFunctionSignatureContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitHlslFunctionDefinition(azslParser::HlslFunctionDefinitionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitHlslFunctionDeclaration(azslParser::HlslFunctionDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitUserDefinedType(azslParser::UserDefinedTypeContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssociatedTypeDeclaration(azslParser::AssociatedTypeDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypedefStatement(azslParser::TypedefStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypealiasStatement(azslParser::TypealiasStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeAliasingDefinitionStatement(azslParser::TypeAliasingDefinitionStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitTypeofExpression(azslParser::TypeofExpressionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericParameterList(azslParser::GenericParameterListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericTypeDefinition(azslParser::GenericTypeDefinitionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitGenericConstraint(azslParser::GenericConstraintContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLanguageDefinedConstraint(azslParser::LanguageDefinedConstraintContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFunctionDeclaration(azslParser::FunctionDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAttributedFunctionDeclaration(azslParser::AttributedFunctionDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFunctionDefinition(azslParser::FunctionDefinitionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAttributedFunctionDefinition(azslParser::AttributedFunctionDefinitionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCompilerExtensionStatement(azslParser::CompilerExtensionStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSrgDefinition(azslParser::SrgDefinitionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAttributedSrgDefinition(azslParser::AttributedSrgDefinitionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSrgMemberDeclaration(azslParser::SrgMemberDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSrgSemantic(azslParser::SrgSemanticContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAttributedSrgSemantic(azslParser::AttributedSrgSemanticContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSrgSemanticBodyDeclaration(azslParser::SrgSemanticBodyDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSrgSemanticMemberDeclaration(azslParser::SrgSemanticMemberDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSamplerBodyDeclaration(azslParser::SamplerBodyDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitSamplerMemberDeclaration(azslParser::SamplerMemberDeclarationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMaxAnisotropyOption(azslParser::MaxAnisotropyOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMinFilterOption(azslParser::MinFilterOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMagFilterOption(azslParser::MagFilterOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMipFilterOption(azslParser::MipFilterOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitReductionTypeOption(azslParser::ReductionTypeOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitComparisonFunctionOption(azslParser::ComparisonFunctionOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAddressUOption(azslParser::AddressUOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAddressVOption(azslParser::AddressVOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAddressWOption(azslParser::AddressWOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMinLodOption(azslParser::MinLodOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMaxLodOption(azslParser::MaxLodOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMipLodBiasOption(azslParser::MipLodBiasOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBorderColorOption(azslParser::BorderColorOptionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitFilterModeEnum(azslParser::FilterModeEnumContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitReductionTypeEnum(azslParser::ReductionTypeEnumContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAddressModeEnum(azslParser::AddressModeEnumContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitComparisonFunctionEnum(azslParser::ComparisonFunctionEnumContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBorderColorEnum(azslParser::BorderColorEnumContext *ctx) override {
    return visitChildren(ctx);
  }


};

