// BLEScanner.cpp : Scans for a Magic Blue BLE bulb, connects to it and sends it commands
//
// Copyright (C) 2016, Uri Shaked. License: MIT.
//
// ***
// See here for info about the bulb protocol: 
// https://medium.com/@urish/reverse-engineering-a-bluetooth-lightbulb-56580fcb7546
// ***
//

#include "stdafx.h"
#include <iostream>
#include <Windows.Foundation.h>
#include <Windows.Devices.Bluetooth.h>
#include <Windows.Devices.Bluetooth.Advertisement.h>
#include <Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <wrl/wrappers/corewrappers.h>
#include <wrl/event.h>
#include <collection.h>
#include <ppltasks.h>
#include <string>
#include <sstream> 
#include <iomanip>

using namespace Platform;
using namespace Windows::Devices;

Enumeration::DeviceWatcher^ w;

std::wstring formatBluetoothAddress(unsigned long long BluetoothAddress) {
	std::wostringstream ret;
	ret << std::hex << std::setfill(L'0')
		<< std::setw(2) << ((BluetoothAddress >> (5 * 8)) & 0xff) << ":"
		<< std::setw(2) << ((BluetoothAddress >> (4 * 8)) & 0xff) << ":"
		<< std::setw(2) << ((BluetoothAddress >> (3 * 8)) & 0xff) << ":"
		<< std::setw(2) << ((BluetoothAddress >> (2 * 8)) & 0xff) << ":"
		<< std::setw(2) << ((BluetoothAddress >> (1 * 8)) & 0xff) << ":"
		<< std::setw(2) << ((BluetoothAddress >> (0 * 8)) & 0xff);
	return ret.str();
}

void connectToDevice(String^ address) {
	String^ queryString = L"(System.Devices.Aep.DeviceAddress:=\"" + address + "\")";
	Windows::Foundation::Collections::IVector<String^>^ requestedProperties = ref new Collections::Vector<String^>();
	requestedProperties->Append(L"System.Devices.Aep.DeviceAddress");
	requestedProperties->Append(L"System.Devices.Aep.IsConnected");
	auto deviceWatcher = Enumeration::DeviceInformation::CreateWatcher(queryString, requestedProperties, Enumeration::DeviceInformationKind::AssociationEndpoint);

	deviceWatcher->Added += ref new Windows::Foundation::TypedEventHandler<Enumeration::DeviceWatcher ^, Enumeration::DeviceInformation ^>(
		[](Enumeration::DeviceWatcher ^watcher, Enumeration::DeviceInformation ^deviceInfo)->void {
		std::wcout << L"BLE Device Information:" << std::endl;
		std::wcout << L"-> Name: " << deviceInfo->Name->Data() << std::endl;
		std::wcout << L"-> Paired? " << (deviceInfo->Pairing->IsPaired ? L"YES" : L"NO") << std::endl;
		if (!deviceInfo->Pairing->IsPaired) {
			auto customPairing = deviceInfo->Pairing->Custom;
			customPairing->PairingRequested += ref new Windows::Foundation::TypedEventHandler<Windows::Devices::Enumeration::DeviceInformationCustomPairing ^, Windows::Devices::Enumeration::DevicePairingRequestedEventArgs ^>(
				[](Enumeration::DeviceInformationCustomPairing ^pairing, Enumeration::DevicePairingRequestedEventArgs ^args) {
				std::wcout << "Device pairing request: " << args->PairingKind.ToString()->Data() << std::endl;
				args->Accept();
			});

			std::wcout << "*** Pairing with the device..." << std::endl;
			auto task = concurrency::create_task(customPairing->PairAsync(Enumeration::DevicePairingKinds::None | Enumeration::DevicePairingKinds::ConfirmOnly));
			task.then([](Enumeration::DevicePairingResult^ result)->void {
				std::wcout << "Device pairing result: " << result->Status.ToString()->Data() << std::endl;
			});
		}
	});

	deviceWatcher->Updated += ref new Windows::Foundation::TypedEventHandler<Windows::Devices::Enumeration::DeviceWatcher ^, Windows::Devices::Enumeration::DeviceInformationUpdate ^>(
		[](Enumeration::DeviceWatcher ^watcher, Enumeration::DeviceInformationUpdate ^deviceInfoUpdate)->void {
		// Nada
	});

	deviceWatcher->Start();
	w = deviceWatcher;
}

int main(Array<String^>^ args) {
	Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);

	auto serviceUUID = Bluetooth::GenericAttributeProfile::GattDeviceService::ConvertShortIdToUuid(0xffe5);
	auto characteristicUUID = Bluetooth::GenericAttributeProfile::GattCharacteristic::ConvertShortIdToUuid(0xffe9);
	Bluetooth::GenericAttributeProfile::GattDeviceService^ deviceService;

	Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher^ bleAdvertisementWatcher = ref new Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher();
	bleAdvertisementWatcher->ScanningMode = Bluetooth::Advertisement::BluetoothLEScanningMode::Active;
	bleAdvertisementWatcher->Received += ref new Windows::Foundation::TypedEventHandler<Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher ^, Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs ^>(
		[serviceUUID, bleAdvertisementWatcher](Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher ^watcher, Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs^ eventArgs) {
		auto serviceUuids = eventArgs->Advertisement->ServiceUuids;
		unsigned int index = -1;
		if (serviceUuids->IndexOf(serviceUUID, &index)) {
			String^ strAddress = ref new String(formatBluetoothAddress(eventArgs->BluetoothAddress).c_str());
			std::wcout << "Target service found on device: " << strAddress->Data() << std::endl;

			connectToDevice(strAddress);

			bleAdvertisementWatcher->Stop();
		}
	});
	bleAdvertisementWatcher->Start();

	// Look for paired devices with the relevant service GUID, and once found start controlling them
	String^ selector = Bluetooth::GenericAttributeProfile::GattDeviceService::GetDeviceSelectorFromUuid(serviceUUID);
	auto gattServiceWatcher = Enumeration::DeviceInformation::CreateWatcher(selector);

	gattServiceWatcher->Added += ref new Windows::Foundation::TypedEventHandler<Enumeration::DeviceWatcher ^, Enumeration::DeviceInformation ^>(
		[characteristicUUID, deviceService](Enumeration::DeviceWatcher ^watcher, Enumeration::DeviceInformation ^deviceInfo)->void {
		std::wcout << L"Found a device with the requested services: " << deviceInfo->Name->Data() << std::endl;
		auto deviceService = concurrency::create_task(Bluetooth::GenericAttributeProfile::GattDeviceService::FromIdAsync(deviceInfo->Id)).get();
		auto characteristic = deviceService->GetCharacteristics(characteristicUUID)->GetAt(0);

		// Set Bulb color to Green
		auto writer = ref new Windows::Storage::Streams::DataWriter();
		auto data = new byte[7]{ 0x56, 0, 0xff, 0, 0x00, 0xf0, 0xaa };
		writer->WriteBytes(ref new Array<byte>(data, 7));
		auto status = concurrency::create_task(characteristic->WriteValueAsync(writer->DetachBuffer(), Bluetooth::GenericAttributeProfile::GattWriteOption::WriteWithoutResponse)).get();
		std::wcout << "Write result: " << status.ToString()->Data() << std::endl;

		for (;;) {
			// Set Bulb color to Yellow
			Sleep(1000);
			writer = ref new Windows::Storage::Streams::DataWriter();
			data = new byte[7]{ 0x56, 0xff, 0xff, 0, 0x00, 0xf0, 0xaa };
			writer->WriteBytes(ref new Array<byte>(data, 7));
			status = concurrency::create_task(characteristic->WriteValueAsync(writer->DetachBuffer(), Bluetooth::GenericAttributeProfile::GattWriteOption::WriteWithoutResponse)).get();
			std::wcout << "Write result: " << status.ToString()->Data() << std::endl;

			// Set Bulb color to Red
			Sleep(1000);
			writer = ref new Windows::Storage::Streams::DataWriter();
			data = new byte[7]{ 0x56, 0xff, 0, 0, 0x00, 0xf0, 0xaa };
			writer->WriteBytes(ref new Array<byte>(data, 7));
			status = concurrency::create_task(characteristic->WriteValueAsync(writer->DetachBuffer(), Bluetooth::GenericAttributeProfile::GattWriteOption::WriteWithoutResponse)).get();
			std::wcout << "Write result: " << status.ToString()->Data() << std::endl;
		}
	});

	gattServiceWatcher->Updated += ref new Windows::Foundation::TypedEventHandler<Windows::Devices::Enumeration::DeviceWatcher ^, Windows::Devices::Enumeration::DeviceInformationUpdate ^>(
		[](Enumeration::DeviceWatcher ^watcher, Enumeration::DeviceInformationUpdate ^deviceInfoUpdate)->void {
		// Nada
	});

	gattServiceWatcher->Start();

	int a;
	std::cin >> a;
	return 0;
}
