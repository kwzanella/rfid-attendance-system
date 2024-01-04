# rfid-attendance-system

## Server

    cd server/
    docker compose up --build

O script executando em `publisher-test` irá enviar pelo tópico `topic` uma mensagem com um id aleatório do intervalo fechado [0, 9]. O script executando em `publisher` irá receber essa mensagem, verificar se o id existe no banco de dados executando no container `database` e mostrar na tela o resultado.

### Interface

Usando o script executando no container `interface` é possível adicionar, remover e visualizar dados gravados no banco de dados no container `database`.

    docker attach interface

## References