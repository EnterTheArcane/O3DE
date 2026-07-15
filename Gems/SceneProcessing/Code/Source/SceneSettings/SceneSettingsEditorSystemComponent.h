/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <IEditor.h>

#include <AzCore/Component/Component.h>
#include <AzCore/EBus/EBus.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzToolsFramework/API/ToolsApplicationAPI.h>

#include <QPointer>

class AssetImporterWindow;
class QMainWindow;

namespace AZ
{
    class AssetBrowserContextProvider;
    class SceneSerializationHandler;
}

//! Python interface for scene settings.
class SceneSettingsAssetImporterForScriptRequests
    : public AZ::EBusTraits
{
public:
    static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
    static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;

    virtual AZ::u64 EditImportSettings(const AZStd::string& sourceFilePath) = 0;
};
using SceneSettingsAssetImporterForScriptRequestBus = AZ::EBus<SceneSettingsAssetImporterForScriptRequests>;

class SceneSettingsEditorSystemComponent;

class SceneSettingsAssetImporterForScriptRequestHandler
    : protected SceneSettingsAssetImporterForScriptRequestBus::Handler
{
public:
    AZ_RTTI(SceneSettingsAssetImporterForScriptRequestHandler, "{C3B9DCFC-CD41-4130-B295-485905A7CECB}");

    explicit SceneSettingsAssetImporterForScriptRequestHandler(SceneSettingsEditorSystemComponent& sceneSettings);
    ~SceneSettingsAssetImporterForScriptRequestHandler();

    static void Reflect(AZ::ReflectContext* context);
    AZ::u64 EditImportSettings(const AZStd::string& sourceFilePath) override;

private:
    SceneSettingsEditorSystemComponent& m_sceneSettings;
};

class SceneSettingsEditorSystemComponent final
    : public AZ::Component
    , private AzToolsFramework::EditorEvents::Bus::Handler
    , private IEditorNotifyListener
{
public:
    AZ_COMPONENT(SceneSettingsEditorSystemComponent, "{8B4B0D3D-5C42-4D4F-A9A6-AB526BFB6C64}");

    SceneSettingsEditorSystemComponent();
    ~SceneSettingsEditorSystemComponent() override;

    static void Reflect(AZ::ReflectContext* context);
    static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);
    static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

    void Activate() override;
    void Deactivate() override;

    QMainWindow* EditImportSettings(const AZStd::string& sourceFilePath);
    QMainWindow* OpenImportSettings();
    bool SaveBeforeClosing();

private:
    void NotifyRegisterViews() override;
    void OnEditorNotifyEvent(EEditorNotifyEvent event) override;

    void Initialize();
    void Shutdown();

    IEditor* m_editor = nullptr;
    QPointer<QMainWindow> m_assetImporterWindow;
    AZStd::unique_ptr<AZ::AssetBrowserContextProvider> m_assetBrowserContextProvider;
    AZStd::unique_ptr<AZ::SceneSerializationHandler> m_sceneSerializationHandler;
    AZStd::unique_ptr<SceneSettingsAssetImporterForScriptRequestHandler> m_requestHandler;
    bool m_initialized = false;
};
