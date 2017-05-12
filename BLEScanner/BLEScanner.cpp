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
#include <wrl/wrappers/corewrappers.h>
#include <wrl/event.h>
#include <collection.h>
#include <ppltasks.h>
#include <string>
#include <sstream> 
#include <iomanip>

using namespace Platform;
using namespace Windows::Devices;

auto serviceUUID = Bluetooth::BluetoothUuidHelper::FromShortId(0xffe5);
auto characteristicUUID = Bluetooth::BluetoothUuidHelper::FromShortId(0xffe9);

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

void connectToBulb(unsigned long long bluetoothAddress) {
	auto leDevice = concurrency::create_task(Bluetooth::BluetoothLEDevice::FromBluetoothAddressAsync(bluetoothAddress)).get();
	auto servicesResult = concurrency::create_task(leDevice->GetGattServicesForUuidAsync(serviceUUID)).get();
	auto service = servicesResult->Services->GetAt(0);
	auto characteristicsResult = concurrency::create_task(service->GetCharacteristicsForUuidAsync(characteristicUUID)).get();
	auto characteristic = characteristicsResult->Characteristics->GetAt(0);

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
}

int main(Array<String^>^ args) {
	Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);

	CoInitializeSecurity(
		nullptr, // TODO: "O:BAG:BAD:(A;;0x7;;;PS)(A;;0x3;;;SY)(A;;0x7;;;BA)(A;;0x3;;;AC)(A;;0x3;;;LS)(A;;0x3;;;NS)"
		-1,
		nullptr,
		nullptr,
		RPC_C_AUTHN_LEVEL_DEFAULT,
		RPC_C_IMP_LEVEL_IDENTIFY,
		NULL,
		EOAC_NONE,
		nullptr);

	Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher^ bleAdvertisementWatcher = ref new Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher();
	bleAdvertisementWatcher->ScanningMode = Bluetooth::Advertisement::BluetoothLEScanningMode::Active;
	bleAdvertisementWatcher->Received += ref new Windows::Foundation::TypedEventHandler<Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher ^, Windows::Devices::Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs ^>(
		[bleAdvertisementWatcher](Bluetooth::Advertisement::BluetoothLEAdvertisementWatcher ^watcher, Bluetooth::Advertisement::BluetoothLEAdvertisementReceivedEventArgs^ eventArgs) {
		auto serviceUuids = eventArgs->Advertisement->ServiceUuids;
		unsigned int index = -1;
		if (serviceUuids->IndexOf(serviceUUID, &index)) {
			String^ strAddress = ref new String(formatBluetoothAddress(eventArgs->BluetoothAddress).c_str());
			std::wcout << "Target service found on device: " << strAddress->Data() << std::endl;

			bleAdvertisementWatcher->Stop();

			connectToBulb(eventArgs->BluetoothAddress);
		}
	});
	bleAdvertisementWatcher->Start();
	
	int a;
	std::cin >> a;
	return 0;
}
