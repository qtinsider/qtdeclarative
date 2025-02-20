/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQMLJSTYPEPROPAGATOR_P_H
#define QQMLJSTYPEPROPAGATOR_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#include <private/qqmljsast_p.h>
#include <private/qqmljsscope_p.h>
#include <private/qqmljscompilepass_p.h>


QT_BEGIN_NAMESPACE

struct QQmlJSTypePropagator : public QQmlJSCompilePass
{
    QQmlJSTypePropagator(const QV4::Compiler::JSUnitGenerator *unitGenerator,
                         const QQmlJSTypeResolver *typeResolver, QQmlJSLogger *logger,
                         QQmlJSTypeInfo *typeInfo = nullptr);

    InstructionAnnotations run(const Function *m_function, QQmlJS::DiagnosticMessage *error);

    void generate_Ret() override;
    void generate_Debug() override;
    void generate_LoadConst(int index) override;
    void generate_LoadZero() override;
    void generate_LoadTrue() override;
    void generate_LoadFalse() override;
    void generate_LoadNull() override;
    void generate_LoadUndefined() override;
    void generate_LoadInt(int value) override;
    void generate_MoveConst(int constIndex, int destTemp) override;
    void generate_LoadReg(int reg) override;
    void generate_StoreReg(int reg) override;
    void generate_MoveReg(int srcReg, int destReg) override;
    void generate_LoadImport(int index) override;
    void generate_LoadLocal(int index) override;
    void generate_StoreLocal(int index) override;
    void generate_LoadScopedLocal(int scope, int index) override;
    void generate_StoreScopedLocal(int scope, int index) override;
    void generate_LoadRuntimeString(int stringId) override;
    void generate_MoveRegExp(int regExpId, int destReg) override;
    void generate_LoadClosure(int value) override;
    void generate_LoadName(int nameIndex) override;
    void generate_LoadGlobalLookup(int index) override;
    void generate_LoadQmlContextPropertyLookup(int index) override;
    void generate_StoreNameSloppy(int nameIndex) override;
    void generate_StoreNameStrict(int name) override;
    void generate_LoadElement(int base) override;
    void generate_StoreElement(int base, int index) override;
    void generate_LoadProperty(int nameIndex) override;
    void generate_LoadOptionalProperty(int name, int offset) override;
    void generate_GetLookup(int index) override;
    void generate_GetOptionalLookup(int index, int offset) override;
    void generate_StoreProperty(int name, int base) override;
    void generate_SetLookup(int index, int base) override;
    void generate_LoadSuperProperty(int property) override;
    void generate_StoreSuperProperty(int property) override;
    void generate_Yield() override;
    void generate_YieldStar() override;
    void generate_Resume(int) override;

    void generate_CallValue(int name, int argc, int argv) override;
    void generate_CallWithReceiver(int name, int thisObject, int argc, int argv) override;
    void generate_CallProperty(int name, int base, int argc, int argv) override;
    void generate_CallPropertyLookup(int lookupIndex, int base, int argc, int argv) override;
    void generate_CallElement(int base, int index, int argc, int argv) override;
    void generate_CallName(int name, int argc, int argv) override;
    void generate_CallPossiblyDirectEval(int argc, int argv) override;
    void generate_CallGlobalLookup(int index, int argc, int argv) override;
    void generate_CallQmlContextPropertyLookup(int index, int argc, int argv) override;
    void generate_CallWithSpread(int func, int thisObject, int argc, int argv) override;
    void generate_TailCall(int func, int thisObject, int argc, int argv) override;
    void generate_Construct(int func, int argc, int argv) override;
    void generate_ConstructWithSpread(int func, int argc, int argv) override;
    void generate_SetUnwindHandler(int offset) override;
    void generate_UnwindDispatch() override;
    void generate_UnwindToLabel(int level, int offset) override;
    void generate_DeadTemporalZoneCheck(int name) override;
    void generate_ThrowException() override;
    void generate_GetException() override;
    void generate_SetException() override;
    void generate_CreateCallContext() override;
    void generate_PushCatchContext(int index, int name) override;
    void generate_PushWithContext() override;
    void generate_PushBlockContext(int index) override;
    void generate_CloneBlockContext() override;
    void generate_PushScriptContext(int index) override;
    void generate_PopScriptContext() override;
    void generate_PopContext() override;
    void generate_GetIterator(int iterator) override;
    void generate_IteratorNext(int value, int done) override;
    void generate_IteratorNextForYieldStar(int iterator, int object) override;
    void generate_IteratorClose(int done) override;
    void generate_DestructureRestElement() override;
    void generate_DeleteProperty(int base, int index) override;
    void generate_DeleteName(int name) override;
    void generate_TypeofName(int name) override;
    void generate_TypeofValue() override;
    void generate_DeclareVar(int varName, int isDeletable) override;
    void generate_DefineArray(int argc, int args) override;
    void generate_DefineObjectLiteral(int internalClassId, int argc, int args) override;
    void generate_CreateClass(int classIndex, int heritage, int computedNames) override;
    void generate_CreateMappedArgumentsObject() override;
    void generate_CreateUnmappedArgumentsObject() override;
    void generate_CreateRestParameter(int argIndex) override;
    void generate_ConvertThisToObject() override;
    void generate_LoadSuperConstructor() override;
    void generate_ToObject() override;
    void generate_Jump(int offset) override;
    void generate_JumpTrue(int offset) override;
    void generate_JumpFalse(int offset) override;
    void generate_JumpNoException(int offset) override;
    void generate_JumpNotUndefined(int offset) override;
    void generate_CheckException() override;
    void generate_CmpEqNull() override;
    void generate_CmpNeNull() override;
    void generate_CmpEqInt(int lhsConst) override;
    void generate_CmpNeInt(int lhs) override;
    void generate_CmpEq(int lhs) override;
    void generate_CmpNe(int lhs) override;
    void generate_CmpGt(int lhs) override;
    void generate_CmpGe(int lhs) override;
    void generate_CmpLt(int lhs) override;
    void generate_CmpLe(int lhs) override;
    void generate_CmpStrictEqual(int lhs) override;
    void generate_CmpStrictNotEqual(int lhs) override;
    void generate_CmpIn(int lhs) override;
    void generate_CmpInstanceOf(int lhs) override;
    void generate_As(int lhs) override;
    void generate_UNot() override;
    void generate_UPlus() override;
    void generate_UMinus() override;
    void generate_UCompl() override;
    void generate_Increment() override;
    void generate_Decrement() override;
    void generate_Add(int lhs) override;
    void generate_BitAnd(int lhs) override;
    void generate_BitOr(int lhs) override;
    void generate_BitXor(int lhs) override;
    void generate_UShr(int lhs) override;
    void generate_Shr(int lhs) override;
    void generate_Shl(int lhs) override;
    void generate_BitAndConst(int rhs) override;
    void generate_BitOrConst(int rhs) override;
    void generate_BitXorConst(int rhs) override;
    void generate_UShrConst(int rhs) override;
    void generate_ShrConst(int rhs) override;
    void generate_ShlConst(int rhs) override;
    void generate_Exp(int lhs) override;
    void generate_Mul(int lhs) override;
    void generate_Div(int lhs) override;
    void generate_Mod(int lhs) override;
    void generate_Sub(int lhs) override;
    void generate_InitializeBlockDeadTemporalZone(int firstReg, int count) override;
    void generate_ThrowOnNullOrUndefined() override;
    void generate_GetTemplateObject(int index) override;

    Verdict startInstruction(QV4::Moth::Instr::Type instr) override;
    void endInstruction(QV4::Moth::Instr::Type instr) override;

private:
    struct ExpectedRegisterState
    {
        int originatingOffset = 0;
        VirtualRegisters registers;
    };

    struct PassState : QQmlJSCompilePass::State
    {
        InstructionAnnotations annotations;
        QSet<int> jumpTargets;
        bool skipInstructionsUntilNextJumpTarget = false;
        bool needsMorePasses = false;
    };

    void handleUnqualifiedAccess(const QString &name) const;
    void checkDeprecated(QQmlJSScope::ConstPtr scope, const QString &name, bool isMethod) const;
    bool checkRestricted(const QString &propertyName) const;
    QQmlJS::SourceLocation getCurrentSourceLocation() const;
    QQmlJS::SourceLocation getCurrentBindingSourceLocation() const;

    void propagateBinaryOperation(QSOperator::Op op, int lhs);
    void propagateCall(const QList<QQmlJSMetaMethod> &methods, int argc, int argv);
    void propagatePropertyLookup(const QString &name);
    void propagateScopeLookupCall(const QString &functionName, int argc, int argv);
    void saveRegisterStateForJump(int offset);
    bool canConvertFromTo(const QQmlJSRegisterContent &from, const QQmlJSRegisterContent &to);

    QString registerName(int registerIndex) const;

    void setRegister(int index, const QQmlJSRegisterContent &content);
    void setRegister(int index, const QQmlJSScope::ConstPtr &content);

    QQmlJSRegisterContent checkedInputRegister(int reg);
    QQmlJSMetaMethod bestMatchForCall(const QList<QQmlJSMetaMethod> &methods, int argc, int argv,
                                      QStringList *errors);

    QQmlJSRegisterContent m_returnType;
    QQmlJSTypeInfo *m_typeInfo = nullptr;

    // Not part of the state, as the back jumps are the reason for running multiple passes
    QMultiHash<int, ExpectedRegisterState> m_jumpOriginRegisterStateByTargetInstructionOffset;

    PassState m_state;
};

QT_END_NAMESPACE

#endif // QQMLJSTYPEPROPAGATOR_P_H
