/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */

// AzNetworking Public Precompiled Header
// Contains ALL public AzNetworking headers. Applied as a PUBLIC PCH so that
// all targets depending on AzNetworking automatically benefit from precompiled headers.
// AzCore's public PCH is inherited through the build dependency chain.
// Controlled by the LY_PCH CMake option (default: ON).

#pragma once
#include <AzNetworking/AzNetworkingModule.h>
#include <AzNetworking/ConnectionLayer/ConnectionEnums.h>
#include <AzNetworking/ConnectionLayer/ConnectionMetrics.h>
#include <AzNetworking/ConnectionLayer/IConnection.h>
#include <AzNetworking/ConnectionLayer/IConnectionListener.h>
#include <AzNetworking/ConnectionLayer/IConnectionSet.h>
#include <AzNetworking/ConnectionLayer/SequenceGenerator.h>
#include <AzNetworking/DataStructures/ByteBuffer.h>
#include <AzNetworking/DataStructures/FixedSizeBitset.h>
#include <AzNetworking/DataStructures/FixedSizeBitsetView.h>
#include <AzNetworking/DataStructures/FixedSizeVectorBitset.h>
#include <AzNetworking/DataStructures/IBitset.h>
#include <AzNetworking/DataStructures/RingBufferBitset.h>
#include <AzNetworking/DataStructures/TimeoutQueue.h>
#include <AzNetworking/Framework/ICompressor.h>
#include <AzNetworking/Framework/INetworking.h>
#include <AzNetworking/Framework/INetworkInterface.h>
#include <AzNetworking/Framework/NetworkingSystemComponent.h>
#include <AzNetworking/Framework/NetworkInterfaceMetrics.h>
#include <AzNetworking/PacketLayer/IPacket.h>
#include <AzNetworking/PacketLayer/IPacketHeader.h>
#include <AzNetworking/Serialization/AbstractValue.h>
#include <AzNetworking/Serialization/AzContainerSerializers.h>
#include <AzNetworking/Serialization/DeltaSerializer.h>
#include <AzNetworking/Serialization/HashSerializer.h>
#include <AzNetworking/Serialization/ISerializer.h>
#include <AzNetworking/Serialization/NetworkInputSerializer.h>
#include <AzNetworking/Serialization/NetworkOutputSerializer.h>
#include <AzNetworking/Serialization/StringifySerializer.h>
#include <AzNetworking/Serialization/TrackChangedSerializer.h>
#include <AzNetworking/Serialization/TypeValidatingSerializer.h>
#include <AzNetworking/TcpTransport/TcpConnection.h>
#include <AzNetworking/TcpTransport/TcpConnectionSet.h>
#include <AzNetworking/TcpTransport/TcpListenThread.h>
#include <AzNetworking/TcpTransport/TcpNetworkInterface.h>
#include <AzNetworking/TcpTransport/TcpPacketHeader.h>
#include <AzNetworking/TcpTransport/TcpRingBuffer.h>
#include <AzNetworking/TcpTransport/TcpRingBufferImpl.h>
#include <AzNetworking/TcpTransport/TcpSocket.h>
#include <AzNetworking/TcpTransport/TcpSocketManager.h>
#include <AzNetworking/TcpTransport/TlsSocket.h>
#include <AzNetworking/UdpTransport/DtlsEndpoint.h>
#include <AzNetworking/UdpTransport/DtlsSocket.h>
#include <AzNetworking/UdpTransport/UdpConnection.h>
#include <AzNetworking/UdpTransport/UdpConnectionSet.h>
#include <AzNetworking/UdpTransport/UdpFragmentQueue.h>
#include <AzNetworking/UdpTransport/UdpHeartbeatThread.h>
#include <AzNetworking/UdpTransport/UdpNetworkInterface.h>
#include <AzNetworking/UdpTransport/UdpPacketHeader.h>
#include <AzNetworking/UdpTransport/UdpPacketIdWindow.h>
#include <AzNetworking/UdpTransport/UdpPacketTracker.h>
#include <AzNetworking/UdpTransport/UdpReaderThread.h>
#include <AzNetworking/UdpTransport/UdpReliableQueue.h>
#include <AzNetworking/UdpTransport/UdpSocket.h>
#include <AzNetworking/Utilities/CidrAddress.h>
#include <AzNetworking/Utilities/EncryptionCommon.h>
#include <AzNetworking/Utilities/Endian.h>
#include <AzNetworking/Utilities/IpAddress.h>
#include <AzNetworking/Utilities/NetworkCommon.h>
#include <AzNetworking/Utilities/NetworkIncludes.h>
#include <AzNetworking/Utilities/QuantizedValues.h>
#include <AzNetworking/Utilities/TimedThread.h>
