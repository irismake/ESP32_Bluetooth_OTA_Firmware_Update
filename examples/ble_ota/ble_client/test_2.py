import asyncio
from bleak import BleakClient

# 연결할 BLE 장치의 주소(UUID)를 여기에 입력하세요.
device_address = "0C:B8:15:EC:91:C6"

async def connect_and_send_data():
    async with BleakClient(device_address) as client:
        print(f"Connected to {device_address}")
        
        # 서비스 및 특성을 검색하고 필요한 특성의 UUID를 여기에 입력하세요.
        # 예: service_uuid = "0000180f-0000-1000-8000-00805f9b34fb"
        #     characteristic_uuid = "00002a19-0000-1000-8000-00805f9b34fb"
        service_uuid = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
        characteristic_uuid = "beb5483e-36e1-4688-b7f5-ea07361b26a8"

        # 서비스 검색
        services = client.services  
        for service in services:
            if service.uuid == service_uuid:
                print(f"Found service: {service.uuid}")

                # 특성 검색
                characteristics = service.characteristics
                for characteristic in characteristics:
                    if characteristic.uuid == characteristic_uuid:
                        print(f"Found characteristic: {characteristic.uuid}")

                        # 데이터 전송 (여기에서 데이터를 작성하고 전송합니다)
                        data_to_send = b"Hello, ESP32!"

                        await client.write_gatt_char(characteristic.uuid, data_to_send)
                 
                        
                        break  # 특성을 찾았으므로 내부 루프 종료
                else:
                    print(f"Characteristic {characteristic_uuid} not found in service {service_uuid}")
                break  # 서비스를 찾았으므로 외부 루프 종료
            else:
                print(f"Service {service_uuid} not found")

        # 연결을 종료하고 리소스를 폐기합니다.
        await client.disconnect()

asyncio.run(connect_and_send_data())
