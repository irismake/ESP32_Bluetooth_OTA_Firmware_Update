import asyncio
from bleak import BleakClient, BleakError

address = "0C:B8:15:EC:91:C6"
read_write_charcteristic_uuid = "beb5483e-36e1-4688-b7f5-ea07361b26a8"

# BLE 장치 연결 상태를 모니터링하기 위한 이벤트 핸들러


async def run():    
    client = BleakClient(address)
    
    try:
        await client.connect()
        print('Connected to BLE device')
       # client.set_disconnected_callback(on_disconnect)  # 연결 해제 이벤트 핸들러 설정
        
        services = await client.get_services()        
        for service in services:
            for characteristic in service.characteristics:
                if characteristic.uuid == read_write_charcteristic_uuid:
                    print("correct")
                   
                    if 'write' in characteristic.properties:
                        read_data = await client.write_gatt_char(characteristic, bytes(b'hello world'))
                        print('write: ', read_data)
               
                    if 'read' in characteristic.properties:
                        read_data = await client.read_gatt_char(characteristic)
                        print('read ', read_data)
                   
                    if 'indicate' in characteristic.properties:
                        read_data = await client.read_gatt_char(characteristic)
                        print('indicate: ', read_data)
                    else:
                        print("no indicate")
                else:
                    print("incorrect")
                    
    except BleakError as e:
        print(f"Error: {e}")
    finally:
        await client.disconnect()
        print('Disconnecting from BLE device')

asyncio.run(run())
print('done')
