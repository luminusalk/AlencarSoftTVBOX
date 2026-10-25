# Plano de validação do aplicativo C — S1009+

Este plano se refere ao **binário C**, não aos testes antigos do add-on
Python. O usuário pediu para suspender alterações/testes no receptor nesta
etapa. Execute apenas revisão de fonte e CI até ele retomar os testes na TV.

## Verificações concluídas

| Caso | Resultado | Alcance |
| --- | --- | --- |
| HTTPS C-SKY com CA e hostname verificados | Segmento de 188000 bytes íntegro no receptor | Transporte, não audiovisual. |
| HLS C-SKY + CGI HTTP + DLNA | Usuário viu imagem e ouviu som por 25 s | Caminho C + firmware; não interface integrada. |
| `showcartoon` com `dvbapp` pausado | Tela preta/sem som, erro 962 | Caminho direto não validado. |
| DVB direto, duas tentativas | Tela preta/sem som | Escrita de TS não basta. |
| CI `40200018458` | Testes Linux e builds C-SKY/MIPS passaram | Inclui mock DLNA, sem hardware. |

## Antes de voltar à box

1. Revisar testes `tests/test_hls_bridge.py` e
   `tests/test_firmware_dlna.py`; acrescentar casos de resposta HTTP/SOAP
   truncada, cliente que desconecta e repetição de Start/Stop se necessário.
2. Rodar o workflow de branch por `workflow_dispatch` e conferir artefato
   `alencarsoft_tvbox_csky`. A CA já está embutida no executável.
3. Preparar atualização reversível do aplicativo próprio. Não tocar em
   `/app/dvbapp`, plugin Python, partições MTD, drivers ou canais de satélite.

## Após autorização expressa para ensaio no receptor

1. Confirmar que `dvbapp` está ativo e descobrir IP/porta atuais. Instalar
   apenas o binário C próprio, conferindo hash e mantendo a versão anterior
   disponível para rollback.
2. Abrir TV Cultura via `--play-url` por tempo limitado. Registrar se o
   renderer foi descoberto, se SOAP aceitou URI/Play, se o HTTP local foi
   lido e, **separadamente**, se o usuário viu imagem **e** ouviu som.
3. Parar normalmente e confirmar retorno do sistema original, sem duplicar
   `dvbapp`. Repetir abrir/parar várias vezes.
4. Abrir a grade, navegar com IR, selecionar OK, voltar com BACK, trocar de
   canal e sair. Observar disputa de IR/OSD e transparência do framebuffer.
5. Testar ao menos uma URL HTTP direta real, um HLS compatível diferente e
   reprodução prolongada. Verificar memória, reconexão e latência de Stop.

Só marcar o aplicativo como funcional quando a interface C inteira cumprir
seleção, imagem, som, parada e retorno à grade. Estado DLNA `PLAYING`, bytes
na FIFO ou sucesso do processo não substituem a observação audiovisual.
