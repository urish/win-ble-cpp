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
#include <experimental/resumable>
#include <pplawait.h>

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

concurrency::task<void> setColor(Bluetooth::GenericAttributeProfile::GattCharacteristic^ characteristic, byte red, byte green, byte blue) {
	auto writer = ref new Windows::Storage::Streams::DataWriter();
	auto data = new byte[7]{ 0x56, red, green, blue, 0x00, 0xf0, 0xaa };
	writer->WriteBytes(ref new Array<byte>(data, 7));
	auto status = co_await characteristic->WriteValueAsync(writer->DetachBuffer(), Bluetooth::GenericAttributeProfile::GattWriteOption::WriteWithoutResponse);
	std::wcout << "Write result: " << status.ToString()->Data() << std::endl;
}

concurrency::task<void> connectToBulb(unsigned long long bluetoothAddress) {
	auto leDevice = co_await Bluetooth::BluetoothLEDevice::FromBluetoothAddressAsync(bluetoothAddress);
	auto servicesResult = co_await leDevice->GetGattServicesForUuidAsync(serviceUUID);
	auto service = servicesResult->Services->GetAt(0);
	auto characteristicsResult = co_await service->GetCharacteristicsForUuidAsync(characteristicUUID);
	auto characteristic = characteristicsResult->Characteristics->GetAt(0);

	co_await setColor(characteristic, 0, 0xff, 0); // Green

	for (;;) {
		Sleep(1000);
		co_await setColor(characteristic, 0xff, 0xff, 0);	// Yellow

		Sleep(1000);
		co_await setColor(characteristic, 0xff, 0, 0);	// Red
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
