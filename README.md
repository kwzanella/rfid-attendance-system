# rfid-attendance-system

## Server

    cd server/
    docker compose up --build

O script executando em `publisher-test` irá enviar pelo tópico `topic` uma mensagem com um id aleatório do intervalo fechado [0, 9]. O script executando em `publisher` irá receber essa mensagem, verificar se o id existe no banco de dados executando no container `database` e mostrar na tela o resultado.

### Interface

Usando o script executando no container `interface` é possível adicionar, remover e visualizar dados gravados no banco de dados no container `database`.

    docker attach interface

## References

https://www.snapeda.com/parts/ESP32-DEVKITC/Espressif%20Systems/view-part/?ref=search&t=esp32

https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/esp32/get-started-devkitc.html

https://www.snapeda.com/parts/RFID-RC522/Handson%20Technology/view-part/?ref=search&t=rc522%20RFID

https://linuxhint.com/power-esp32/
