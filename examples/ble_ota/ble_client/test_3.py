import asyncio
from bleak import BleakClient

async def run(address, loop):
    async with BleakClient(address, loop=loop) as client:
        # Replace the following with your service and characteristic UUIDs
        service_uuid = "00001801-0000-1000-8000-00805f9b34fb"
        tx_char_uuid = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
        rx_char_uuid = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

        services = await client.get_services()

        tx_char = None
        rx_char = None

        for service in services:
            if service.uuid == service_uuid:
                characteristics = service.characteristics
                for char in characteristics:
                    if char.uuid == tx_char_uuid:
                        tx_char = char
                    elif char.uuid == rx_char_uuid:
                        rx_char = char

        if tx_char is None or rx_char is None:
            print("Couldn't find the specified service or characteristic.")
            return

        while True:
            data = input("Enter data to send: ")
            await client.write_gatt_char(tx_char.handle, data.encode('utf-8'), response=True)

            # Wait for notifications
            received_data = await client.read_gatt_char(rx_char.handle)
            print("Received data:", received_data.decode('utf-8'))

if __name__ == "__main__":
    address = "0C:B8:15:EC:91:C6"  # Replace with the MAC address of your ESP32
    loop = asyncio.get_event_loop()
    loop.run_until_complete(run(address, loop))
