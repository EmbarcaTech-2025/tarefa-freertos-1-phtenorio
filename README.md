
# Tarefa: Roteiro de FreeRTOS #1 - EmbarcaTech 2025

Autor: Pedro Henrique Oliveira

Curso: Residência Tecnológica em Sistemas Embarcados

Instituição: EmbarcaTech - HBr

Campinas, Junho de 2025

---
Respostas para as perguntas do trablalho.

**O que acontece se todas as tarefas tiverem a mesma prioridade?**

Elas rodam em um esquema de rodízio (Round-Robin), também conhecido como Time-Slicing (Fatiamento de Tempo).
O sistema operacional dá uma pequena "fatia de tempo" para cada tarefa executar.
Quando o tempo de uma tarefa acaba, o sistema a interrompe e dá a vez para a próxima da fila, garantindo que nenhuma tarefa monopolize o processador.


**Qual tarefa consome mais tempo da CPU?**

No código, todas as três tarefas têm um consumo de CPU baixíssimo.
O consumo de CPU ocorre apenas no trabalho executado entre as chamadas de vTaskDelay.
Como as tarefas passam a maior parte do tempo "dormindo" no vTaskDelay, o processador fica a maior parte do tempo (>99%) executando a tarefa ociosa (idle task) do FreeRTOS, que é o estado de menor atividade.


**Quais seriam os riscos de usar polling sem prioridades?**

Usar "polling" (verificação em loop) com todas as tarefas na mesma prioridade gera três riscos principais:
Alta Latência: A resposta a um evento (como um botão) se torna lenta e imprevisível, pois a tarefa precisa esperar sua vez no rodízio para verificar o estado. Eventos rápidos podem ser perdidos.
Desperdício de CPU e Energia: As tarefas estão sempre rodando e consumindo ciclos de processamento apenas para verificar se há algo a fazer, mesmo que na maioria das vezes não haja.
Falta de Urgência: É impossível garantir que uma tarefa crítica (ler um sensor de emergência) seja executada antes de uma tarefa menos importante (atualizar um display). Todas são tratadas como iguais.

---

**# Projeto Multitarefa com FreeRTOS na Raspberry Pi Pico**

Este projeto é uma demonstração prática do uso do sistema operacional de tempo real (RTOS) FreeRTOS em uma placa Raspberry Pi Pico. Ele gerencia três tarefas concorrentes para controlar periféricos de hardware (LED, Buzzer) com base na interação do usuário (Botões).

O objetivo é servir como um exemplo claro de conceitos fundamentais de um RTOS, como:

Criação e gerenciamento de tarefas com diferentes prioridades.
Controle de estado de tarefas (vTaskSuspend, vTaskResume).
Comunicação segura entre tarefas usando Filas de Mensagens (xQueue).
Uso de hardware de PWM para geração de tons.
Estrutura de um projeto embarcado modular.
Funcionalidades
Tarefa de LED: Um LED RGB cicla continuamente entre as cores vermelho, verde e azul. Esta tarefa pode ser pausada e retomada.
Tarefa de Buzzer: Um buzzer passivo emite bipes periódicos com um tom puro de 2700 Hz, gerado via PWM. Esta tarefa pode ser ligada, desligada e silenciada.
Tarefa de Botões: Uma tarefa de alta prioridade monitora dois botões para controlar as outras duas tarefas, garantindo uma interface de usuário responsiva.
Botão A: Pausa/retoma a animação do LED e atua como um "mute" de emergência para o buzzer.
Botão B: Liga ou desliga o ciclo de bipes do buzzer.

**Arquitetura do Software**
O sistema é orquestrado pelo FreeRTOS e dividido em três tarefas principais:

led_task (Prioridade 1):

Responsável unicamente por ciclar as cores do LED em um loop infinito.
É uma tarefa "burra", que não sabe da existência das outras. Seu estado (pausado ou ativo) é controlado diretamente pela button_task através das funções vTaskSuspend() e vTaskResume().
buzzer_task (Prioridade 1):

Responsável por gerar o som.
Esta tarefa passa a maior parte do tempo bloqueada, esperando por um comando. Ela não usa flags globais.
A comunicação é feita via uma Fila de Comandos (xQueue). Ela só executa uma ação quando a button_task envia um comando para esta fila. Isso desacopla as tarefas e torna o sistema mais robusto.
button_task (Prioridade 2):

Por ter a maior prioridade, garante que a resposta aos toques do usuário seja imediata, "interrompendo" as outras tarefas se necessário (preempção).
Lê o estado dos botões e age como o "cérebro" do sistema, enviando ordens para as outras duas tarefas.

---

## 📜 Licença
GNU GPL-3.0.
