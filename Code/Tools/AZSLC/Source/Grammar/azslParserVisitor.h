
// Generated from azslParser.g4 by ANTLR 4.13.2

#pragma once


#include "Antlr4.h"
#include "azslParser.h"



/**
 * This class defines an abstract visitor for a parse tree
 * produced by azslParser.
 */
class  azslParserVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by azslParser.
   */
    virtual std::any visitCompilationUnit(azslParser::CompilationUnitContext *context) = 0;

    virtual std::any visitTopLevelDeclaration(azslParser::TopLevelDeclarationContext *context) = 0;

    virtual std::any visitIdExpression(azslParser::IdExpressionContext *context) = 0;

    virtual std::any visitUnqualifiedId(azslParser::UnqualifiedIdContext *context) = 0;

    virtual std::any visitQualifiedId(azslParser::QualifiedIdContext *context) = 0;

    virtual std::any visitNestedNameSpecifier(azslParser::NestedNameSpecifierContext *context) = 0;

    virtual std::any visitClassDefinitionStatement(azslParser::ClassDefinitionStatementContext *context) = 0;

    virtual std::any visitClassDefinition(azslParser::ClassDefinitionContext *context) = 0;

    virtual std::any visitBaseList(azslParser::BaseListContext *context) = 0;

    virtual std::any visitClassMemberDeclaration(azslParser::ClassMemberDeclarationContext *context) = 0;

    virtual std::any visitStructDefinitionStatement(azslParser::StructDefinitionStatementContext *context) = 0;

    virtual std::any visitStructDefinition(azslParser::StructDefinitionContext *context) = 0;

    virtual std::any visitStructMemberDeclaration(azslParser::StructMemberDeclarationContext *context) = 0;

    virtual std::any visitAnyStructuredTypeDefinitionStatement(azslParser::AnyStructuredTypeDefinitionStatementContext *context) = 0;

    virtual std::any visitEnumDefinitionStatement(azslParser::EnumDefinitionStatementContext *context) = 0;

    virtual std::any visitEnumDefinition(azslParser::EnumDefinitionContext *context) = 0;

    virtual std::any visitUnscopedEnum(azslParser::UnscopedEnumContext *context) = 0;

    virtual std::any visitScopedEnum(azslParser::ScopedEnumContext *context) = 0;

    virtual std::any visitEnumeratorListDefinition(azslParser::EnumeratorListDefinitionContext *context) = 0;

    virtual std::any visitEnumeratorDeclarator(azslParser::EnumeratorDeclaratorContext *context) = 0;

    virtual std::any visitAnyStructuredTypeDefinition(azslParser::AnyStructuredTypeDefinitionContext *context) = 0;

    virtual std::any visitInterfaceDefinitionStatement(azslParser::InterfaceDefinitionStatementContext *context) = 0;

    virtual std::any visitInterfaceDefinition(azslParser::InterfaceDefinitionContext *context) = 0;

    virtual std::any visitInterfaceMemberDeclaration(azslParser::InterfaceMemberDeclarationContext *context) = 0;

    virtual std::any visitConstantBufferTemplated(azslParser::ConstantBufferTemplatedContext *context) = 0;

    virtual std::any visitVariableDeclarationStatement(azslParser::VariableDeclarationStatementContext *context) = 0;

    virtual std::any visitFunctionParams(azslParser::FunctionParamsContext *context) = 0;

    virtual std::any visitFunctionParam(azslParser::FunctionParamContext *context) = 0;

    virtual std::any visitHlslSemantic(azslParser::HlslSemanticContext *context) = 0;

    virtual std::any visitHlslSemanticName(azslParser::HlslSemanticNameContext *context) = 0;

    virtual std::any visitAttributeArguments(azslParser::AttributeArgumentsContext *context) = 0;

    virtual std::any visitAttributeArgumentList(azslParser::AttributeArgumentListContext *context) = 0;

    virtual std::any visitGlobalAttribute(azslParser::GlobalAttributeContext *context) = 0;

    virtual std::any visitAttachedAttribute(azslParser::AttachedAttributeContext *context) = 0;

    virtual std::any visitAttributeSpecifier(azslParser::AttributeSpecifierContext *context) = 0;

    virtual std::any visitAttributeSpecifierSequence(azslParser::AttributeSpecifierSequenceContext *context) = 0;

    virtual std::any visitAttributeSpecifierAny(azslParser::AttributeSpecifierAnyContext *context) = 0;

    virtual std::any visitBlock(azslParser::BlockContext *context) = 0;

    virtual std::any visitStatement(azslParser::StatementContext *context) = 0;

    virtual std::any visitForInitializer(azslParser::ForInitializerContext *context) = 0;

    virtual std::any visitCaseSwitchLabel(azslParser::CaseSwitchLabelContext *context) = 0;

    virtual std::any visitDefaultSwitchLabel(azslParser::DefaultSwitchLabelContext *context) = 0;

    virtual std::any visitSwitchSection(azslParser::SwitchSectionContext *context) = 0;

    virtual std::any visitSwitchBlock(azslParser::SwitchBlockContext *context) = 0;

    virtual std::any visitEmptyStatement(azslParser::EmptyStatementContext *context) = 0;

    virtual std::any visitBlockStatement(azslParser::BlockStatementContext *context) = 0;

    virtual std::any visitExpressionStatement(azslParser::ExpressionStatementContext *context) = 0;

    virtual std::any visitIfStatement(azslParser::IfStatementContext *context) = 0;

    virtual std::any visitSwitchStatement(azslParser::SwitchStatementContext *context) = 0;

    virtual std::any visitWhileStatement(azslParser::WhileStatementContext *context) = 0;

    virtual std::any visitDoStatement(azslParser::DoStatementContext *context) = 0;

    virtual std::any visitForStatement(azslParser::ForStatementContext *context) = 0;

    virtual std::any visitBreakStatement(azslParser::BreakStatementContext *context) = 0;

    virtual std::any visitContinueStatement(azslParser::ContinueStatementContext *context) = 0;

    virtual std::any visitDiscardStatement(azslParser::DiscardStatementContext *context) = 0;

    virtual std::any visitReturnStatement(azslParser::ReturnStatementContext *context) = 0;

    virtual std::any visitExtenstionStatement(azslParser::ExtenstionStatementContext *context) = 0;

    virtual std::any visitTypeAliasingDefinitionStatementLabel(azslParser::TypeAliasingDefinitionStatementLabelContext *context) = 0;

    virtual std::any visitElseClause(azslParser::ElseClauseContext *context) = 0;

    virtual std::any visitParenthesizedExpression(azslParser::ParenthesizedExpressionContext *context) = 0;

    virtual std::any visitMemberAccessExpression(azslParser::MemberAccessExpressionContext *context) = 0;

    virtual std::any visitPrefixUnaryExpression(azslParser::PrefixUnaryExpressionContext *context) = 0;

    virtual std::any visitLiteralExpression(azslParser::LiteralExpressionContext *context) = 0;

    virtual std::any visitConditionalExpression(azslParser::ConditionalExpressionContext *context) = 0;

    virtual std::any visitPostfixUnaryExpression(azslParser::PostfixUnaryExpressionContext *context) = 0;

    virtual std::any visitNumericConstructorExpression(azslParser::NumericConstructorExpressionContext *context) = 0;

    virtual std::any visitFunctionCallExpression(azslParser::FunctionCallExpressionContext *context) = 0;

    virtual std::any visitIdentifierExpression(azslParser::IdentifierExpressionContext *context) = 0;

    virtual std::any visitBinaryExpression(azslParser::BinaryExpressionContext *context) = 0;

    virtual std::any visitAssignmentExpression(azslParser::AssignmentExpressionContext *context) = 0;

    virtual std::any visitCastExpression(azslParser::CastExpressionContext *context) = 0;

    virtual std::any visitArrayAccessExpression(azslParser::ArrayAccessExpressionContext *context) = 0;

    virtual std::any visitOtherExpression(azslParser::OtherExpressionContext *context) = 0;

    virtual std::any visitCommaExpression(azslParser::CommaExpressionContext *context) = 0;

    virtual std::any visitPostfixUnaryOperator(azslParser::PostfixUnaryOperatorContext *context) = 0;

    virtual std::any visitPrefixUnaryOperator(azslParser::PrefixUnaryOperatorContext *context) = 0;

    virtual std::any visitBinaryOperator(azslParser::BinaryOperatorContext *context) = 0;

    virtual std::any visitAssignmentOperator(azslParser::AssignmentOperatorContext *context) = 0;

    virtual std::any visitArgumentList(azslParser::ArgumentListContext *context) = 0;

    virtual std::any visitArguments(azslParser::ArgumentsContext *context) = 0;

    virtual std::any visitVariableDeclaration(azslParser::VariableDeclarationContext *context) = 0;

    virtual std::any visitVariableDeclarators(azslParser::VariableDeclaratorsContext *context) = 0;

    virtual std::any visitUnnamedVariableDeclarator(azslParser::UnnamedVariableDeclaratorContext *context) = 0;

    virtual std::any visitNamedVariableDeclarator(azslParser::NamedVariableDeclaratorContext *context) = 0;

    virtual std::any visitVariableInitializer(azslParser::VariableInitializerContext *context) = 0;

    virtual std::any visitStandardVariableInitializer(azslParser::StandardVariableInitializerContext *context) = 0;

    virtual std::any visitArrayElementInitializers(azslParser::ArrayElementInitializersContext *context) = 0;

    virtual std::any visitArrayRankSpecifier(azslParser::ArrayRankSpecifierContext *context) = 0;

    virtual std::any visitPackOffsetNode(azslParser::PackOffsetNodeContext *context) = 0;

    virtual std::any visitStorageFlags(azslParser::StorageFlagsContext *context) = 0;

    virtual std::any visitStorageFlag(azslParser::StorageFlagContext *context) = 0;

    virtual std::any visitType(azslParser::TypeContext *context) = 0;

    virtual std::any visitPredefinedType(azslParser::PredefinedTypeContext *context) = 0;

    virtual std::any visitSubobjectType(azslParser::SubobjectTypeContext *context) = 0;

    virtual std::any visitOtherViewResourceType(azslParser::OtherViewResourceTypeContext *context) = 0;

    virtual std::any visitRtxBuiltInTypes(azslParser::RtxBuiltInTypesContext *context) = 0;

    virtual std::any visitBufferPredefinedType(azslParser::BufferPredefinedTypeContext *context) = 0;

    virtual std::any visitBufferType(azslParser::BufferTypeContext *context) = 0;

    virtual std::any visitByteAddressBufferTypes(azslParser::ByteAddressBufferTypesContext *context) = 0;

    virtual std::any visitPatchPredefinedType(azslParser::PatchPredefinedTypeContext *context) = 0;

    virtual std::any visitPatchType(azslParser::PatchTypeContext *context) = 0;

    virtual std::any visitSamplerStatePredefinedType(azslParser::SamplerStatePredefinedTypeContext *context) = 0;

    virtual std::any visitScalarType(azslParser::ScalarTypeContext *context) = 0;

    virtual std::any visitStreamOutputPredefinedType(azslParser::StreamOutputPredefinedTypeContext *context) = 0;

    virtual std::any visitStreamOutputObjectType(azslParser::StreamOutputObjectTypeContext *context) = 0;

    virtual std::any visitStructuredBufferPredefinedType(azslParser::StructuredBufferPredefinedTypeContext *context) = 0;

    virtual std::any visitStructuredBufferName(azslParser::StructuredBufferNameContext *context) = 0;

    virtual std::any visitTextureType(azslParser::TextureTypeContext *context) = 0;

    virtual std::any visitTexturePredefinedType(azslParser::TexturePredefinedTypeContext *context) = 0;

    virtual std::any visitGenericTexturePredefinedType(azslParser::GenericTexturePredefinedTypeContext *context) = 0;

    virtual std::any visitTextureTypeMS(azslParser::TextureTypeMSContext *context) = 0;

    virtual std::any visitMsTexturePredefinedType(azslParser::MsTexturePredefinedTypeContext *context) = 0;

    virtual std::any visitSubpassInputType(azslParser::SubpassInputTypeContext *context) = 0;

    virtual std::any visitSubpassInputPredefinedType(azslParser::SubpassInputPredefinedTypeContext *context) = 0;

    virtual std::any visitGenericSubpassInputPredefinedType(azslParser::GenericSubpassInputPredefinedTypeContext *context) = 0;

    virtual std::any visitVectorType(azslParser::VectorTypeContext *context) = 0;

    virtual std::any visitGenericVectorType(azslParser::GenericVectorTypeContext *context) = 0;

    virtual std::any visitScalarOrVectorType(azslParser::ScalarOrVectorTypeContext *context) = 0;

    virtual std::any visitScalarOrVectorOrMatrixType(azslParser::ScalarOrVectorOrMatrixTypeContext *context) = 0;

    virtual std::any visitMatrixType(azslParser::MatrixTypeContext *context) = 0;

    virtual std::any visitGenericMatrixPredefinedType(azslParser::GenericMatrixPredefinedTypeContext *context) = 0;

    virtual std::any visitRegisterAllocation(azslParser::RegisterAllocationContext *context) = 0;

    virtual std::any visitSamplerStateProperty(azslParser::SamplerStatePropertyContext *context) = 0;

    virtual std::any visitLiteral(azslParser::LiteralContext *context) = 0;

    virtual std::any visitLeadingTypeFunctionSignature(azslParser::LeadingTypeFunctionSignatureContext *context) = 0;

    virtual std::any visitHlslFunctionDefinition(azslParser::HlslFunctionDefinitionContext *context) = 0;

    virtual std::any visitHlslFunctionDeclaration(azslParser::HlslFunctionDeclarationContext *context) = 0;

    virtual std::any visitUserDefinedType(azslParser::UserDefinedTypeContext *context) = 0;

    virtual std::any visitAssociatedTypeDeclaration(azslParser::AssociatedTypeDeclarationContext *context) = 0;

    virtual std::any visitTypedefStatement(azslParser::TypedefStatementContext *context) = 0;

    virtual std::any visitTypealiasStatement(azslParser::TypealiasStatementContext *context) = 0;

    virtual std::any visitTypeAliasingDefinitionStatement(azslParser::TypeAliasingDefinitionStatementContext *context) = 0;

    virtual std::any visitTypeofExpression(azslParser::TypeofExpressionContext *context) = 0;

    virtual std::any visitGenericParameterList(azslParser::GenericParameterListContext *context) = 0;

    virtual std::any visitGenericTypeDefinition(azslParser::GenericTypeDefinitionContext *context) = 0;

    virtual std::any visitGenericConstraint(azslParser::GenericConstraintContext *context) = 0;

    virtual std::any visitLanguageDefinedConstraint(azslParser::LanguageDefinedConstraintContext *context) = 0;

    virtual std::any visitFunctionDeclaration(azslParser::FunctionDeclarationContext *context) = 0;

    virtual std::any visitAttributedFunctionDeclaration(azslParser::AttributedFunctionDeclarationContext *context) = 0;

    virtual std::any visitFunctionDefinition(azslParser::FunctionDefinitionContext *context) = 0;

    virtual std::any visitAttributedFunctionDefinition(azslParser::AttributedFunctionDefinitionContext *context) = 0;

    virtual std::any visitCompilerExtensionStatement(azslParser::CompilerExtensionStatementContext *context) = 0;

    virtual std::any visitSrgDefinition(azslParser::SrgDefinitionContext *context) = 0;

    virtual std::any visitAttributedSrgDefinition(azslParser::AttributedSrgDefinitionContext *context) = 0;

    virtual std::any visitSrgMemberDeclaration(azslParser::SrgMemberDeclarationContext *context) = 0;

    virtual std::any visitSrgSemantic(azslParser::SrgSemanticContext *context) = 0;

    virtual std::any visitAttributedSrgSemantic(azslParser::AttributedSrgSemanticContext *context) = 0;

    virtual std::any visitSrgSemanticBodyDeclaration(azslParser::SrgSemanticBodyDeclarationContext *context) = 0;

    virtual std::any visitSrgSemanticMemberDeclaration(azslParser::SrgSemanticMemberDeclarationContext *context) = 0;

    virtual std::any visitSamplerBodyDeclaration(azslParser::SamplerBodyDeclarationContext *context) = 0;

    virtual std::any visitSamplerMemberDeclaration(azslParser::SamplerMemberDeclarationContext *context) = 0;

    virtual std::any visitMaxAnisotropyOption(azslParser::MaxAnisotropyOptionContext *context) = 0;

    virtual std::any visitMinFilterOption(azslParser::MinFilterOptionContext *context) = 0;

    virtual std::any visitMagFilterOption(azslParser::MagFilterOptionContext *context) = 0;

    virtual std::any visitMipFilterOption(azslParser::MipFilterOptionContext *context) = 0;

    virtual std::any visitReductionTypeOption(azslParser::ReductionTypeOptionContext *context) = 0;

    virtual std::any visitComparisonFunctionOption(azslParser::ComparisonFunctionOptionContext *context) = 0;

    virtual std::any visitAddressUOption(azslParser::AddressUOptionContext *context) = 0;

    virtual std::any visitAddressVOption(azslParser::AddressVOptionContext *context) = 0;

    virtual std::any visitAddressWOption(azslParser::AddressWOptionContext *context) = 0;

    virtual std::any visitMinLodOption(azslParser::MinLodOptionContext *context) = 0;

    virtual std::any visitMaxLodOption(azslParser::MaxLodOptionContext *context) = 0;

    virtual std::any visitMipLodBiasOption(azslParser::MipLodBiasOptionContext *context) = 0;

    virtual std::any visitBorderColorOption(azslParser::BorderColorOptionContext *context) = 0;

    virtual std::any visitFilterModeEnum(azslParser::FilterModeEnumContext *context) = 0;

    virtual std::any visitReductionTypeEnum(azslParser::ReductionTypeEnumContext *context) = 0;

    virtual std::any visitAddressModeEnum(azslParser::AddressModeEnumContext *context) = 0;

    virtual std::any visitComparisonFunctionEnum(azslParser::ComparisonFunctionEnumContext *context) = 0;

    virtual std::any visitBorderColorEnum(azslParser::BorderColorEnumContext *context) = 0;


};

