// BLEScanner.cpp : Scans for a Magic Blue BLE bulb, connects to it and sends it commands
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

using namespace Platform;
using namespace Windows::Devices;

#define TARGET_DEVICE_NAME L"LEDBLE-5BF8CCEF" 

int main(Array<String^>^ args) {
	Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);

	auto serviceUUID = Bluetooth::GenericAttributeProfile::GattDeviceService::ConvertShortIdToUuid(0xffe5);
	auto characteristicUUID = Bluetooth::GenericAttributeProfile::GattCharacteristic::ConvertShortIdToUuid(0xffe9);
	Bluetooth::GenericAttributeProfile::GattDeviceService^ deviceService;

	String^ queryString = L"(System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\")";
	Windows::Foundation::Collections::IVector<String^>^ requestedProperties = ref new Collections::Vector<String^>();
	requestedProperties->Append(L"System.Devices.Aep.DeviceAddress");
	requestedProperties->Append(L"System.Devices.Aep.IsConnected");
	auto deviceWatcher = Enumeration::DeviceInformation::CreateWatcher(queryString, requestedProperties, Enumeration::DeviceInformationKind::AssociationEndpoint);

	deviceWatcher->Added += ref new Windows::Foundation::TypedEventHandler<Enumeration::DeviceWatcher ^, Enumeration::DeviceInformation ^>(
		[](Enumeration::DeviceWatcher ^watcher, Enumeration::DeviceInformation ^deviceInfo)->void {
		std::wcout << L"Found some BLE device" << std::endl;
		String^ localName = deviceInfo->Name;
		String^ prop = safe_cast<String^>(deviceInfo->Properties->Lookup(L"System.Devices.Aep.DeviceAddress"));
		Boolean connected = safe_cast<Boolean>(deviceInfo->Properties->Lookup(L"System.Devices.Aep.IsConnected"));

		if (localName) {
			std::wcout << L"Device name:" << deviceInfo->Name->Data() << std::endl;
			std::wcout << L"Adress:" << prop->Data() << std::endl;
			std::wcout << L"Paired? " << (deviceInfo->Pairing->IsPaired ? L"YES" : L"NO") << std::endl;

			std::wcout << L"Connected? " << (connected ? L"YES" : L"NO") << std::endl;

			if (localName->Equals(TARGET_DEVICE_NAME)) {
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

			std::wcout << L"---" << std::endl;
		}
	}
	);

	deviceWatcher->Updated += ref new Windows::Foundation::TypedEventHandler<Windows::Devices::Enumeration::DeviceWatcher ^, Windows::Devices::Enumeration::DeviceInformationUpdate ^>(
		[](Enumeration::DeviceWatcher ^watcher, Enumeration::DeviceInformationUpdate ^deviceInfoUpdate)->void {
		std::wcout << "Device Watcher: Updated" << std::endl;
	});

	deviceWatcher->Start();


	String^ selector = Bluetooth::GenericAttributeProfile::GattDeviceService::GetDeviceSelectorFromUuid(serviceUUID);
	auto gattServiceWatcher = Enumeration::DeviceInformation::CreateWatcher(selector);

	gattServiceWatcher->Added += ref new Windows::Foundation::TypedEventHandler<Enumeration::DeviceWatcher ^, Enumeration::DeviceInformation ^>(
		[characteristicUUID, deviceService](Enumeration::DeviceWatcher ^watcher, Enumeration::DeviceInformation ^deviceInfo)->void {
		std::wcout << L"Found a device with the requested services: " << deviceInfo->Name->Data() << std::endl;
		auto deviceService = concurrency::create_task(Bluetooth::GenericAttributeProfile::GattDeviceService::FromIdAsync(deviceInfo->Id)).get();
		auto characteristic = deviceService->GetCharacteristics(characteristicUUID)->GetAt(0);

		auto writer = ref new Windows::Storage::Streams::DataWriter();
		auto data = new byte[7]{ 0x56, 0, 0xff, 0, 0x00, 0xf0, 0xaa };
		writer->WriteBytes(ref new Array<byte>(data, 7));
		auto status = concurrency::create_task(characteristic->WriteValueAsync(writer->DetachBuffer(), Bluetooth::GenericAttributeProfile::GattWriteOption::WriteWithoutResponse)).get();
		std::wcout << "Write result: " << status.ToString()->Data() << std::endl;

		for (;;) {
			Sleep(1000);
			writer = ref new Windows::Storage::Streams::DataWriter();
			data = new byte[7]{ 0x56, 0xff, 0xff, 0, 0x00, 0xf0, 0xaa };
			writer->WriteBytes(ref new Array<byte>(data, 7));
			status = concurrency::create_task(characteristic->WriteValueAsync(writer->DetachBuffer(), Bluetooth::GenericAttributeProfile::GattWriteOption::WriteWithoutResponse)).get();
			std::wcout << "Write result: " << status.ToString()->Data() << std::endl;
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

