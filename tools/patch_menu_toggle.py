with open(r'D:\EMULADORES\Git\AlencarSoftTVBOX\AlencarSoftTVBOX-Clang\src\main.c', 'rb') as f:
    c = f.read()

# 1. Update VIEW_PLAYING key handler to include OK
old1 = b'if (key == KEY_ACTION_BACK || key == KEY_EXIT_APP || key == KEY_ACTION_MENU || key == KEY_ACTION_SAT) {'
new1 = b'if (key == KEY_ACTION_BACK || key == KEY_EXIT_APP || key == KEY_ACTION_MENU || key == KEY_ACTION_SAT || key == KEY_ACTION_OK) {'
assert old1 in c, 'old1 not found'
c = c.replace(old1, new1, 1)

# 2. Update MENU and BACK handler in grid
old2 = (b'        case KEY_ACTION_BACK:\n'
        b'        case KEY_EXIT_APP:\n'
        b'            /* Na grade de canais, abre o dialogo de confirmacao de saida */\n'
        b'            app->view = VIEW_CONFIRM_EXIT;\n'
        b'            app->exit_dialog_choice = 1; /* Padrao selecionado: NAO */\n'
        b'            break;\n'
        b'\n'
        b'        case KEY_ACTION_MENU:\n'
        b'            if (app->active_playing_idx >= 0 &&\n'
        b'                player_get_state() == PLAYER_STATE_PLAYING) {\n'
        b'                app->view = VIEW_PLAYING;\n'
        b'                vfd_set_text(app->catalog.channels[app->active_playing_idx].name);\n'
        b'            }\n'
        b'            break;')

c_crlf = c.replace(b'\r\n', b'\n')
if old2 in c_crlf:
    new2 = (b'        case KEY_ACTION_BACK:\n'
            b'            if (app->active_playing_idx >= 0) {\n'
            b'                printf("[%s] [BACK] Retornando ao canal ativo em tela cheia...\\n", APP_NAME);\n'
            b'                app->view = VIEW_PLAYING;\n'
            b'                vfd_set_text(app->catalog.channels[app->active_playing_idx].name);\n'
            b'                fb_clear(&app->fb, 0x00000000);\n'
            b'                fb_flip(&app->fb);\n'
            b'                break;\n'
            b'            }\n'
            b'            app->view = VIEW_CONFIRM_EXIT;\n'
            b'            app->exit_dialog_choice = 1;\n'
            b'            break;\n'
            b'\n'
            b'        case KEY_EXIT_APP:\n'
            b'            /* Na grade de canais, abre o dialogo de confirmacao de saida */\n'
            b'            app->view = VIEW_CONFIRM_EXIT;\n'
            b'            app->exit_dialog_choice = 1; /* Padrao selecionado: NAO */\n'
            b'            break;\n'
            b'\n'
            b'        case KEY_ACTION_MENU:\n'
            b'            /* Alternador (Toggle) do MENU: fecha a grade mantendo o canal ativo */\n'
            b'            if (app->active_playing_idx >= 0) {\n'
            b'                printf("[%s] [MENU] Fechando grade e mantendo canal ativo em tela cheia...\\n", APP_NAME);\n'
            b'                app->view = VIEW_PLAYING;\n'
            b'                vfd_set_text(app->catalog.channels[app->active_playing_idx].name);\n'
            b'                fb_clear(&app->fb, 0x00000000);\n'
            b'                fb_flip(&app->fb);\n'
            b'            }\n'
            b'            break;')
    is_crlf = b'\r\n' in c
    if is_crlf:
        c = c_crlf.replace(old2, new2).replace(b'\n', b'\r\n')
    else:
        c = c_crlf.replace(old2, new2)
    with open(r'D:\EMULADORES\Git\AlencarSoftTVBOX\AlencarSoftTVBOX-Clang\src\main.c', 'wb') as f:
        f.write(c)
    print('main.c atualizado com sucesso!')
else:
    print('old2 nao encontrado!')
