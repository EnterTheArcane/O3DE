/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 */

#pragma once

#include <AzCore/RTTI/TypeInfo.h>

namespace Box3D
{
    inline constexpr AZ::TypeId SystemComponentTypeId{"{573EC8F6-59D1-4B1B-92A3-97604466B50D}"};
    inline constexpr AZ::TypeId ModuleTypeId{"{7DA32521-65F9-43CC-B69B-71E7ABFB8C98}"};
    inline constexpr AZ::TypeId SystemConfigurationTypeId{"{46CD595B-0C6C-4C5A-B5D6-07A8E9A96738}"};
    inline constexpr AZ::TypeId RigidBodyComponentTypeId{"{1670B987-AE71-46B6-ADBC-EC86601DC63B}"};
    inline constexpr AZ::TypeId RigidBodyConfigurationTypeId{"{C4DF1AE4-35D9-4190-AAC8-A71B0F7604E9}"};
    inline constexpr AZ::TypeId StaticRigidBodyComponentTypeId{"{10F7D391-52D2-4989-AD9A-A9284A8795F6}"};
    inline constexpr AZ::TypeId ColliderComponentTypeId{"{9A0461B7-8339-4FC7-89C9-FC562AEB3B66}"};
    inline constexpr AZ::TypeId HeightfieldColliderComponentTypeId{"{2CC0A340-9382-4DB7-82D9-5E719F9BB701}"};
    inline constexpr AZ::TypeId ExplosionComponentTypeId{"{47B04F4F-2C62-45FE-B6FB-B611CA367186}"};
    inline constexpr AZ::TypeId WindComponentTypeId{"{E1F63F13-8636-43AB-BFC4-F2044853DEB4}"};
    inline constexpr AZ::TypeId JointComponentTypeId{"{C24065F7-BD6F-4DB0-A50D-20AB6ABBE903}"};
    inline constexpr AZ::TypeId CharacterControllerComponentTypeId{"{09E847DA-F6C7-4BDA-99D1-2F6375067383}"};
    inline constexpr AZ::TypeId CharacterConfigurationTypeId{"{5E89BB8E-E3D2-4833-8071-E421D0A0495E}"};
    inline constexpr AZ::TypeId CharacterSupportTypeId{"{4F09F898-1B31-4C68-A98A-D79BCBC423D9}"};
    inline constexpr AZ::TypeId CharacterStateTypeId{"{70616E10-AFC6-4B25-92BC-D94B4C0B903B}"};
    inline constexpr AZ::TypeId MaterialTypeId{"{013C5212-D28C-4A14-8B67-1A02E0D2B433}"};
    inline constexpr AZ::TypeId MaterialConfigurationTypeId{"{7FCF68C8-21B5-4EFD-B3BA-3934723E37A4}"};
    inline constexpr AZ::TypeId MaterialResultTypeId{"{2D769D46-F681-4F33-8241-65D1E86F686B}"};
    inline constexpr AZ::TypeId MaterialHandleCollectionTypeId{"{1E69391A-8A34-4A8F-986D-6E50394F57BE}"};
    inline constexpr AZ::TypeId MaterialConfigurationCollectionTypeId{"{52EA4F01-A6A5-451F-B6F1-1F2D70C52809}"};
    inline constexpr AZ::TypeId DebugMaterialPresetTypeId{"{FC9A29BC-7EFA-4F36-8BBB-00E808638E80}"};
    inline constexpr AZ::TypeId CompoundChildTypeId{"{760D18D6-7AD1-40CE-8F49-88FF429E8BC6}"};
    inline constexpr AZ::TypeId ISystemTypeId{"{50B7A9DE-5C12-4038-AF6C-85818D6B9CCB}"};
    inline constexpr AZ::TypeId IWorldQueriesTypeId{"{E0698835-97C8-4B2D-9278-C6A41B50935B}"};
    inline constexpr AZ::TypeId ICookingTypeId{"{715D51DB-8BE1-4B77-97AC-390320360628}"};
    inline constexpr AZ::TypeId GeometryHitTypeId{"{FCF0DC9B-FAD6-45DD-8B96-C6B5BCB4B91D}"};
    inline constexpr AZ::TypeId RaycastRequestCollectionTypeId{"{B46BFF0A-C0CA-49D9-B21C-243727C61F38}"};
    inline constexpr AZ::TypeId ClosestQueryResultCollectionTypeId{"{9F9D56AF-3D45-4912-8B6C-497ECE02379B}"};
    inline constexpr AZ::TypeId CookedRaycastResultTypeId{"{B05D1374-43C9-4E25-837C-C4840BE887EF}"};
    inline constexpr AZ::TypeId IDiagnosticsTypeId{"{63EDDA5F-2092-49B0-8C72-5AB126704C03}"};
    inline constexpr AZ::TypeId StepProfileSnapshotTypeId{"{B1913E6E-788A-4EBB-9B79-32DD43383418}"};
    inline constexpr AZ::TypeId StatisticsSnapshotTypeId{"{D2AF356E-0717-4534-BCAD-8E1A3FF96790}"};
    inline constexpr AZ::TypeId SimulationCountersTypeId{"{D368E0F3-960C-4E2B-B84A-5E207AD5E993}"};
    inline constexpr AZ::TypeId CapacityHighWaterMarksTypeId{"{26B8AB1A-E322-4437-9E44-2DDDE4D74BCA}"};
    inline constexpr AZ::TypeId RecordingDataTypeId{"{6FA0149C-A99A-4B91-BF4D-4372298BD1A2}"};
    inline constexpr AZ::TypeId RecordingResultTypeId{"{ED0BF644-A625-47CE-8EEF-91DB0E05256D}"};
    inline constexpr AZ::TypeId IEffectsTypeId{"{94780C27-D94F-4DDE-B7E7-CDB7E499303B}"};
    inline constexpr AZ::TypeId IDebugRendererTypeId{"{C37CE517-C0D7-4EC6-9890-C2FF2EB1AFD8}"};
    inline constexpr AZ::TypeId IReplayTypeId{"{20429D07-C374-4D22-A70E-771DD29C09EC}"};
    inline constexpr AZ::TypeId ReplayInfoTypeId{"{46598CBB-8698-47A5-8D9F-B8EC5563F6AB}"};
    inline constexpr AZ::TypeId ReplayBodyTypeId{"{5F65E7D6-51DB-4977-97C3-BF333277D832}"};
    inline constexpr AZ::TypeId ReplayQueryTypeId{"{B96DD49A-E112-40E8-86ED-897B7A769C8C}"};
    inline constexpr AZ::TypeId ReplayQueryHitTypeId{"{01E55901-FA3C-49B7-9AE7-549B960BA49F}"};
    inline constexpr AZ::TypeId ReplayInfoResultTypeId{"{6D8330AE-632D-41BB-ADF2-9CDA690DA1FF}"};
    inline constexpr AZ::TypeId ReplayBodyResultTypeId{"{85D10B0C-5B0C-42B0-B381-F2FC464AA6FC}"};
    inline constexpr AZ::TypeId ReplayQueryResultTypeId{"{E1C8FC0D-537B-4CAC-8C73-C748A06CBD9A}"};
    inline constexpr AZ::TypeId ReplayQueryHitResultTypeId{"{DF072AEE-13A9-4C9F-9EBB-D803F7088DEB}"};
    inline constexpr AZ::TypeId ReplayShapeIdTypeId{"{7C8E1ABA-884F-41CE-A5F1-0838C2097554}"};
    inline constexpr AZ::TypeId BodyStateTypeId{"{916553FD-2A8A-49AB-BAB6-31DDB8E99D88}"};
    inline constexpr AZ::TypeId BodyPropertiesTypeId{"{638EADE6-3AA5-4E1D-98B9-5FC2813F844F}"};
    inline constexpr AZ::TypeId MassPropertiesTypeId{"{43BC8E48-A9E0-4439-9FC4-E5B6DD0E2633}"};
    inline constexpr AZ::TypeId ShapeStateTypeId{"{791226C2-463E-40AF-AE05-285663D9037E}"};
    inline constexpr AZ::TypeId ClosestPointTypeId{"{AABDC388-4451-4B80-B632-D0C802DEFB37}"};
    inline constexpr AZ::TypeId JointThresholdEventTypeId{"{D953EF85-B5F4-45D1-981C-7EA8942F1CB3}"};
    inline constexpr AZ::TypeId ContactPointTypeId{"{D86D12D4-61E4-476F-8EE7-013D57FE6331}"};
    inline constexpr AZ::TypeId ContactEventTypeId{"{FB5B4A23-A40B-466E-A8E3-A50C8106292F}"};
    inline constexpr AZ::TypeId ContactHitEventTypeId{"{CA27D5BA-C18A-4D03-93C5-F5A63A153068}"};
    inline constexpr AZ::TypeId SensorEventTypeId{"{9836FBA5-C9B4-4445-B701-CB3AE49926AF}"};
    inline constexpr AZ::TypeId BodyMoveEventTypeId{"{D1E68C5A-3B96-48EA-B5F0-940F91A32290}"};
} // namespace Box3D

namespace Box3D::Editor
{
    inline constexpr AZ::TypeId ModuleTypeId{"{91D2E642-4709-460D-AFE9-728D49741345}"};
    inline constexpr AZ::TypeId ColliderComponentTypeId{"{6DC30C18-8146-4354-9C32-82C4239C0495}"};
    inline constexpr AZ::TypeId HeightfieldColliderComponentTypeId{"{E97C57DF-1DC6-4B21-A3FE-0D68ABDC7CEA}"};
    inline constexpr AZ::TypeId ExplosionComponentTypeId{"{8CC44861-D5B5-4C8C-AF11-AF26F6E28040}"};
    inline constexpr AZ::TypeId WindComponentTypeId{"{8FBA7B8C-BA38-4D3D-84B6-259893325D28}"};
    inline constexpr AZ::TypeId RigidBodyComponentTypeId{"{65552919-767C-40BD-81BB-601304139860}"};
    inline constexpr AZ::TypeId StaticRigidBodyComponentTypeId{"{B5035034-A73C-48F4-87EF-F49A95C1B1E8}"};
    inline constexpr AZ::TypeId CharacterControllerComponentTypeId{"{D6028E1C-E681-463A-B2B5-FEF789D82BE8}"};
    inline constexpr AZ::TypeId ParallelJointComponentTypeId{"{717D9CE4-30C5-4C84-8AC8-CDD7A677506F}"};
    inline constexpr AZ::TypeId DistanceJointComponentTypeId{"{011EB7F1-BB1E-4234-98D0-5C40DBC56CAC}"};
    inline constexpr AZ::TypeId FilterJointComponentTypeId{"{D395EA69-7E9F-4CBD-B0DB-CBC2F556C28E}"};
    inline constexpr AZ::TypeId MotorJointComponentTypeId{"{D379EDB7-4252-40F5-A9C9-A23A6F88F26B}"};
    inline constexpr AZ::TypeId PrismaticJointComponentTypeId{"{95ED118D-D24A-478C-BC5C-2E1FF1E8F212}"};
    inline constexpr AZ::TypeId RevoluteJointComponentTypeId{"{D84E8B92-441B-4E68-9CF2-F79065165B4B}"};
    inline constexpr AZ::TypeId SphericalJointComponentTypeId{"{693F1E0D-BA58-466B-8EB6-11CDEC6D5360}"};
    inline constexpr AZ::TypeId WeldJointComponentTypeId{"{A397B07D-31AA-414B-A669-148458941E81}"};
    inline constexpr AZ::TypeId WheelJointComponentTypeId{"{90AD7ECD-8AD8-41D2-9F03-9D596CB6DA27}"};
} // namespace Box3D::Editor
