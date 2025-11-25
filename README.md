# PSI-Microcontroladores2-Aula12
Atividade: Sincronismo, Detecção de Colisão e Integridade


## Etapa 1: Modelagem e Planejamento de Testes

Considerando o cenário proposto de comunicação entre duas placas com modo de operação simples de 5 segundos para transmitir e 5 segundos para receber, é natural que ocorram problemas de sincronismo: uma placa pode acabar transmitindo enquanto a outra está transmitindo também, e mesmo no recebimento podemos não receber a mensagem completa.

### 1.1. Sincronismo por Botão

O diagrama de estados abaixo foi elaborado para demonstrar o funcionamento da passagem de estado de cada microcontrolador:

Diagrama: <img width="1148" height="873" alt="image" src="https://github.com/user-attachments/assets/c26b20c8-f16e-42be-8c90-c646592f75de" />



Apartir desse planejamento, espera-se que as placas consigam não sofrer colisão e funcionar de maneira síncrona, sem necessitar de um botão que se pressione externamente, já que ela sempre checa a sincronização de ambas as placas na passagem de estados apartir do sinal que a placa mestre envia pelo botão.

Para verificar o funcionamento correto do código, testou-se o sincronismo entre as placas utilizando um funcionamento por LEDs, onde o LED azul indica que a placa está apenas com o TX enviando a mensagem enquanto o LED verde indica que a placa está apenas com o RX ativo e está recebendo as mensagens. Elaborando esse teste como TDD, deseja-se testar se as placas estão sincronizadas. Primeiramente, o código falha se qualquer um dos leds não for diferente do LED da outra placa à qualquer momento, já que elas nunca podem estar no mesmo estado ao mesmo tempo.  

| | Teste de Sincronia |
| ----- | ----|
| Pré-Condição | Ambas as placas começam em estados diferentes |
| Etapas de Teste | No momento que qualquer uma das placas trocar de estado, resetar uma placa. |
| Pós-Condição | A placa precisa estar no LED oposto ao LED da outra placa (Nesse caso, se uma placa estiver verde, a outra precisa estar azul e vice-versa. |

Testou-se também se as placas sincronizavam mesmo resetando elas em tempos diferentes. Os testes podem ser vistos no vídeo a seguir:


https://github.com/user-attachments/assets/4d33eeb9-d415-47ee-8040-3090cb11e4ef


### 1.2. Detecção de Colisão

  Sobre a detecção de colisão, não seria necessária essa detecção pelo fato de nossa implemetação com botão não necessitar de uma pessoa externa para sincronizar. Isso faz com que o sincronismo seja automático
  e consiga coordenar sempre o estado do funcionamento de cada placa. Mas, apesar da nossa implementação não necessitar de detector de colisões, fez-se um código para detectar colisões e o diagrama de estados para
  demonstrar como as placas devem funcionar para saber se devem falar ou não.

  O diagrama de estados está logo abaixo para mostrar quais devem ser os estados e condições que a placa deve seguir para garantir a detecção por colisões:

Diagrama: <img width="1576" height="1432" alt="image" src="https://github.com/user-attachments/assets/ea261e2d-9783-401c-bede-0a48ab551a83" />



_Descreva um teste para verificação de correto funcionamento do sistema considerando este requisito de detecção de colisão, contemplando pré-condição, etapas do teste e pós-condição, de forma similar ao realizado em atividades anteriores (Dica: é possível mapear os estados mais relevantes a comportamentos do led da placa para observar o seu funcionamento?)_.

### 1.3. Verificação de Integridade

Reflita inicialmente o que ocorre com as mensagens transmitidas e recebidas em caso de colisão.

Nos casos em que há problemas de colisão, as mensagens podem não ser recebidas de forma completa.
Para lidar com este problema, a proposta é elaborar uma verificação de integridade: no início da mensagem, podemos enviar um hash da mensagem ou pelo menos o tamanho total da mansagem em caracteres, para que o receptor possa verificar se recebeu todos os caracteres de forma íntegra.
Questão para reflexão: _a verificação de integridade de conteúdo é suportada pela verificação de tamanho da mensagem recebida em caracteres?_

_Elabore um diagrama de transição de estados (versão 3) para modelar como as duas placas irão interagir com o sincronismo por botão, a detecção de colisão e a verificação de integridade, considerando os diversos estados possíveis e os eventos que determinam as transições de estados (vocês podem utilizar o D2 diagrams visto em atividade anterior: https://play.d2lang.com/)_.

_Descreva um teste para verificação de correto funcionamento do sistema considerando este requisito de verificação de integridade, contemplando pré-condição, etapas do teste e pós-condição, de forma similar ao realizado em atividades anteriores (Dica: podemos mapear a correta verificação de integridade a comportamentos da placa?)_.

## Etapa 2: Desenvolvimento Orientado a Testes

A partir da modelagem realizada e dos testes planejados, faça o desenvolvimento da solução para contemplar os 3 requisitos e passar nos 3 testes descritos.

O uso de IA Generativa é incentivado: _veja a diferença entre fazer prompts sem fornecer os requisitos e testes planejados, ou usar prompts com os diagramas e testes planejados_.

Além dos testes de cada requisito em cada etapa, faça **testes de regressão** também, para garantir que os requisitos das etapas anteriores estão funcionando (Dica: podemos ter modos de operação diferentes para testar diferentes features e não nos confundirmos com os comportamentos dos leds em cada situação).
Isto é: se o sincronismo continua funcionando após a integração da detecção de colisão, e se o sincronismo e a detecção continuam funcionando após a adição da verificação de integridade.

_Faça o upload de todos os códigos no repositório_ (pode ser em branches diferentes, ou até organizar em pull requests as diferentes features).

_Vocês devem adicionar todas as evidências de funcionamento (como por exemplo capturas de tela e fotos) dos testes realizados, mostrando todos os testes realizados no README.
As imagens e outras evidências de funcionamento devem estar descritas no README e devem estar em uma pasta chamada "results" no repositório._

### 2.1. Sincronismo por Botão

Insira aqui as descrições dos resultados e referencie as fotos e capturas de tela que mostram o funcionamento.

### 2.2. Detecção de Colisão

Insira aqui as descrições dos resultados e referencie as fotos e capturas de tela que mostram o funcionamento.

### 2.3. Verificação de Integridade

Insira aqui as descrições dos resultados e referencie as fotos e capturas de tela que mostram o funcionamento.
