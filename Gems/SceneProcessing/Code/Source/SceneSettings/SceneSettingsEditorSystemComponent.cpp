/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#include <SceneSettings/SceneSettingsEditorSystemComponent.h>

#include <SceneSettings/AssetBrowserContextProvider.h>
#include <SceneSettings/AssetImporterWindow.h>
#include <SceneSettings/ImporterRootDisplay.h>
#include <SceneSettings/SceneSettingsSerializationHandler.h>

#include <AzCore/Component/ComponentApplication.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>
#include <SceneAPI/SceneCore/Events/AssetImportRequest.h>

SceneSettingsEditorSystemComponent::SceneSettingsEditorSystemComponent() = default;
SceneSettingsEditorSystemComponent::~SceneSettingsEditorSystemComponent() = default;

void SceneSettingsEditorSystemComponent::Reflect(AZ::ReflectContext* context)
{
    SceneSettingsAssetImporterForScriptRequestHandler::Reflect(context);
    SceneSettingsRootDisplayScriptRequestHandler::Reflect(context);

    if (auto serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
    {
        serializeContext->Class<SceneSettingsEditorSystemComponent, AZ::Component>()
            ->Version(0);
    }
}

void SceneSettingsEditorSystemComponent::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
{
    provided.push_back(AZ_CRC_CE("SceneSettingsEditorService"));
}

void SceneSettingsEditorSystemComponent::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
{
    incompatible.push_back(AZ_CRC_CE("SceneSettingsEditorService"));
}

void SceneSettingsEditorSystemComponent::Activate()
{
    AzToolsFramework::EditorEvents::Bus::Handler::BusConnect();
}

void SceneSettingsEditorSystemComponent::Deactivate()
{
    Shutdown();
    if (m_editor)
    {
        m_editor->UnregisterNotifyListener(this);
        m_editor = nullptr;
    }
    AzToolsFramework::EditorEvents::Bus::Handler::BusDisconnect();
}

void SceneSettingsEditorSystemComponent::NotifyRegisterViews()
{
    if (!m_editor)
    {
        AzToolsFramework::EditorRequests::Bus::BroadcastResult(m_editor, &AzToolsFramework::EditorRequests::GetEditor);
        if (m_editor)
        {
            m_editor->RegisterNotifyListener(this);
        }
    }

    Initialize();
}

void SceneSettingsEditorSystemComponent::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
    if (event == eNotify_OnQuit)
    {
        Shutdown();
        if (m_editor)
        {
            m_editor->UnregisterNotifyListener(this);
            m_editor = nullptr;
        }
    }
}

void SceneSettingsEditorSystemComponent::Initialize()
{
    if (m_initialized)
    {
        return;
    }

    m_sceneSerializationHandler = AZStd::make_unique<AZ::SceneSerializationHandler>();
    m_sceneSerializationHandler->Activate();
    m_assetBrowserContextProvider = AZStd::make_unique<AZ::AssetBrowserContextProvider>(*this);
    m_requestHandler = AZStd::make_unique<SceneSettingsAssetImporterForScriptRequestHandler>(*this);

    AzToolsFramework::ToolsApplicationRequestBus::Broadcast(
        &AzToolsFramework::ToolsApplicationRequests::CreateAndAddEntityFromComponentTags,
        AZStd::vector<AZ::Crc32>({ AZ::SceneAPI::Events::AssetImportRequest::GetAssetImportRequestComponentTag() }),
        "AssetImportersEntity");

    m_initialized = true;
}

void SceneSettingsEditorSystemComponent::Shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    m_requestHandler.reset();
    m_assetBrowserContextProvider.reset();
    if (m_assetImporterWindow)
    {
        delete m_assetImporterWindow;
        m_assetImporterWindow = nullptr;
    }
    m_sceneSerializationHandler->Deactivate();
    m_sceneSerializationHandler.reset();
    m_initialized = false;
}

QMainWindow* SceneSettingsEditorSystemComponent::EditImportSettings(const AZStd::string& sourceFilePath)
{
    if (auto assetImporterWindow = qobject_cast<AssetImporterWindow*>(m_assetImporterWindow))
    {
        assetImporterWindow->OpenFile(sourceFilePath);
        return m_assetImporterWindow;
    }
    return nullptr;
}

QMainWindow* SceneSettingsEditorSystemComponent::OpenImportSettings()
{
    if (m_assetImporterWindow)
    {
        return nullptr;
    }

    m_assetImporterWindow = new AssetImporterWindow();
    return m_assetImporterWindow;
}

bool SceneSettingsEditorSystemComponent::SaveBeforeClosing()
{
    if (auto assetImporterWindow = qobject_cast<AssetImporterWindow*>(m_assetImporterWindow))
    {
        return assetImporterWindow->CanClose();
    }
    return false;
}

SceneSettingsAssetImporterForScriptRequestHandler::SceneSettingsAssetImporterForScriptRequestHandler(
    SceneSettingsEditorSystemComponent& sceneSettings)
    : m_sceneSettings(sceneSettings)
{
    SceneSettingsAssetImporterForScriptRequestBus::Handler::BusConnect();
}

SceneSettingsAssetImporterForScriptRequestHandler::~SceneSettingsAssetImporterForScriptRequestHandler()
{
    SceneSettingsAssetImporterForScriptRequestBus::Handler::BusDisconnect();
}

void SceneSettingsAssetImporterForScriptRequestHandler::Reflect(AZ::ReflectContext* context)
{
    if (auto behaviorContext = azrtti_cast<AZ::BehaviorContext*>(context))
    {
        behaviorContext->EBus<SceneSettingsAssetImporterForScriptRequestBus>("SceneSettingsAssetImporterForScriptRequestBus")
            ->Attribute(AZ::Script::Attributes::Category, "Asset Importer")
            ->Event("EditImportSettings", &SceneSettingsAssetImporterForScriptRequestBus::Events::EditImportSettings);
    }
}

AZ::u64 SceneSettingsAssetImporterForScriptRequestHandler::EditImportSettings(const AZStd::string& sourceFilePath)
{
    QMainWindow* importSettingsWindow = m_sceneSettings.EditImportSettings(sourceFilePath);
    return importSettingsWindow ? importSettingsWindow->winId() : 0;
}
