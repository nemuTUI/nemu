#include <nm_core.h>
#include <nm_string.h>
#include <nm_vector.h>
#include <nm_window.h>
#include <nm_ncurses.h>

#include <time.h>

#define NM_SI_ENEMIE_COUNT  60
#define NM_SI_ENEMIE_INROW  10
#define NM_SI_EMEMIE_ROTATE 10
#define NM_SI_DELAY         20000
#define NM_SI_BULLETS       200
#define NM_SI_BONIS_DELAY   1000
#define NM_SI_GAIN_COUNT    500

static void nm_si_game(void)
{
    typedef enum {
        NM_SI_SHIP_1,
        NM_SI_SHIP_2,
        NM_SI_SHIP_3,
        NM_SI_BULLET,
        NM_SI_PLAYER,
        NM_SI_BONUS
    } nm_si_type_t;

    typedef struct {
        int pos_x;
        int pos_y;
        int health;
        bool hit;
        nm_si_type_t type;
    } nm_si_t;

    static bool si_player_init = false;
    static size_t bullets_cnt;
    static nm_si_t player;
    static size_t score;
    static size_t level;
    static size_t speed;
    static size_t gain;

    bool play = true, change_dir = false, next = false;
    int max_x, max_y, start_x, start_y;
    nm_vect_t pl_bullets = NM_INIT_VECT;
    nm_vect_t en_bullets = NM_INIT_VECT;
    nm_vect_t enemies = NM_INIT_VECT;
    nm_str_t info = NM_INIT_STR;
    bool bonus_active = false;
    bool gain_active = false;
    size_t gain_counter = 0;
    int direction = 1, mch;
    int b_direction = 1;
    uint64_t iter = 0;
    nm_si_t bonus;

    const char *shape_player = "<^>";
    const char *shape_ship0  = "#v#";
    const char *shape_ship1  = ")v(";
    const char *shape_ship2  = "]v[";
    const char *shape_dead   = "x-x";
    const char *shape_hit    = ">-<";
    const char *shape_win    = "^_^";
    const char *shape_bonus  = "(?)";

    nodelay(action_window, TRUE);
    getmaxyx(action_window, max_y, max_x);

    bonus = (nm_si_t) {
        .pos_x = 1,
        .pos_y = 3,
        .health = 1,
        .hit = false,
        .type = NM_SI_BONUS
    };

    if (!si_player_init) {
        player = (nm_si_t) {
            .pos_x = max_x / 2,
            .pos_y = max_y - 2,
            .health = 5,
            .hit = false,
            .type = NM_SI_PLAYER
        };
        score = 0, bullets_cnt = NM_SI_BULLETS;
        si_player_init = true;
        speed = NM_SI_EMEMIE_ROTATE;
        level = 0;
        gain = 0;
    } else {
        level++;
        bullets_cnt += (NM_SI_BULLETS - 50);
        if (speed > 1) {
            speed--;
        }
    }
    start_x = (max_x / 5);
    start_y = 5;

    for (size_t n = 0; n < NM_SI_ENEMIE_COUNT; n++) {
        nm_si_type_t type = (n < 10) ? NM_SI_SHIP_3 :
                            (n >= 10 && n < 30) ? NM_SI_SHIP_2 : NM_SI_SHIP_1;
        size_t health = (n < 10) ? 3 :
                            (n >= 10 && n < 30) ? 2 : 1;
        nm_si_t ship = (nm_si_t) {
            .pos_x = start_x,
            .pos_y = start_y,
            .type = type,
            .hit = false,
            .health = health
        };

        nm_vect_insert(&enemies, &ship, sizeof(nm_si_t), NULL);
        start_x += 5;
        if (!((n + 1) % NM_SI_ENEMIE_INROW)) {
            start_y++;
            start_x = (max_x / 5);
        }
    }

    while (play) {
        int ch = wgetch(action_window);

        werase(action_window);
        nm_str_format(&info, "Level %zu [score: %zu hp: %d ammo: %zu ap: %zu]",
                level, score, player.health, bullets_cnt, gain);
        nm_init_action(info.data);

        switch (ch) {
        case KEY_LEFT:
        case NM_KEY_A:
            if (player.pos_x > 1) {
                player.pos_x -= 1;
            }
            break;
        case KEY_RIGHT:
        case NM_KEY_D:
            if (player.pos_x < max_x - 4) {
                player.pos_x += 1;
            }
            break;
        case NM_KEY_S:
            if (gain && !gain_active) {
                gain_counter = NM_SI_GAIN_COUNT;
                gain_active = true;
                gain--;
            }
            break;
        case ' ':
            {
                nm_si_t bullet = (nm_si_t) {
                    .pos_x = player.pos_x + 1,
                    .pos_y = max_y - 3,
                    .health = (gain_active) ? 3 : 1,
                    .hit = false,
                    .type = NM_SI_BULLET,
                };

                if (bullets_cnt) {
                    nm_vect_insert(&pl_bullets, &bullet, sizeof(nm_si_t), NULL);
                    bullets_cnt--;
                }
            }
            break;
        case NM_KEY_Q:
            play = false;
            break;
        case ERR:
        default:
            break;
        }

        if (!enemies.n_memb) {
            play = false;
            next = true;
            mvwaddstr(action_window, player.pos_y, player.pos_x, shape_win);
            break;
        }

        for (size_t n = 0; n < enemies.n_memb; n++) {
            nm_si_t *e = nm_vect_at(&enemies, n);

            for (size_t nb = 0; nb < pl_bullets.n_memb; nb++) {
                nm_si_t *b = nm_vect_at(&pl_bullets, nb);
                size_t hp_hit = e->health;

                if (((b->pos_x >= e->pos_x && b->pos_x <= e->pos_x + 2)) &&
                        (e->pos_y == b->pos_y)) {
                    e->health -= b->health;
                    b->health -= hp_hit;
                    if (e->health <= 0) {
                        switch (e->type) {
                            case NM_SI_SHIP_1:
                                score += 10;
                                break;
                            case NM_SI_SHIP_2:
                                score += 20;
                                break;
                            case NM_SI_SHIP_3:
                                score += 30;
                                break;
                            default:
                                break;
                        }
                    } else {
                        e->hit = true;
                        switch (e->type) {
                            case NM_SI_SHIP_1:
                                score += 1;
                                break;
                            case NM_SI_SHIP_2:
                                score += 2;
                                break;
                            case NM_SI_SHIP_3:
                                score += 3;
                                break;
                            default:
                                break;
                        }
                    }
                    if (b->health <= 0) {
                        nm_vect_delete(&pl_bullets, nb, NULL);
                        nb--;
                    }
                    break;
                }
            }

            if (e->health > 0) {
                bool can_shoot = true;
                const char *shape;
                switch (e->type) {
                case NM_SI_SHIP_1:
                    shape = shape_ship0;
                    break;
                case NM_SI_SHIP_2:
                    shape = shape_ship1;
                    break;
                case NM_SI_SHIP_3:
                    shape = shape_ship2;
                    break;
                default:
                    break;
                }

                if (e->hit) {
                    shape = shape_hit;
                    e->hit = false;
                }

                mvwaddstr(action_window, e->pos_y, e->pos_x, shape);
                if (iter % (speed + 1) == speed) {
                    direction ? e->pos_x++ : e->pos_x--;
                    if (!direction && e->pos_x == 1) {
                        change_dir = true;
                    } else if (direction && (e->pos_x == max_x - 4)) {
                        change_dir = true;
                    }
                }

                for (size_t no = n; no < enemies.n_memb; no++) {
                    nm_si_t *eo = nm_vect_at(&enemies, no);
                    if (((eo->pos_y - 1 >= e->pos_y &&
                          eo->pos_y - 1 <= e->pos_y + 5)) &&
                          (e->pos_x == eo->pos_x)) {
                        can_shoot = false;
                        break;
                    }
                }

                if (can_shoot) {
                    struct timespec ts;
                    int r;

                    clock_gettime(CLOCK_MONOTONIC, &ts);
                    srand((time_t) ts.tv_nsec);
                    r = rand() % 1000;
                    if (r > 498 && r < 500) { /* fire! */
                        nm_si_t bullet = (nm_si_t) {
                            .pos_x = e->pos_x + 1,
                            .pos_y = e->pos_y + 1,
                            .health = 0,
                            .hit = false,
                            .type = NM_SI_BULLET
                        };
                        nm_vect_insert(&en_bullets, &bullet,
                                sizeof(nm_si_t), NULL);
                    }
                }

            } else {
                mvwaddstr(action_window, e->pos_y, e->pos_x, shape_dead);
                nm_vect_delete(&enemies, n, NULL);
                n--;
            }
        }

        if (enemies.n_memb && change_dir) {
            for (size_t n = 0; n < enemies.n_memb; n++) {
                nm_si_t *e = nm_vect_at(&enemies, n);
                e->pos_y += 1;
                if (e->pos_y == max_y - 2) {
                    play = false;
                }
            }

            if (!direction) {
                direction = 1;
            } else {
                direction = 0;
            }
            change_dir = false;
        }

        for (size_t n = 0; n < pl_bullets.n_memb; n++) {
            nm_si_t *b = nm_vect_at(&pl_bullets, n);
            mvwaddch(action_window, b->pos_y, b->pos_x, gain_active ? '^' : '*');
            b->pos_y--;

            if (b->pos_y == 1) {
                nm_vect_delete(&pl_bullets, n, NULL);
                n--;
            }
        }

        if (iter % (NM_SI_BONIS_DELAY + 1) == NM_SI_BONIS_DELAY &&
                !bonus_active) {
            bonus_active = true;
        }

        if (bonus_active) {
            for (size_t n = 0; n < pl_bullets.n_memb; n++) {
                nm_si_t *b = nm_vect_at(&pl_bullets, n);

                if (((b->pos_x >= bonus.pos_x && b->pos_x <= bonus.pos_x + 2)) &&
                        (bonus.pos_y == b->pos_y)) {
                    bonus.hit = true;
                    b->health--;
                    if (!b->health) {
                        nm_vect_delete(&pl_bullets, n, NULL);
                    }
                    break;
                }
            }

            if (bonus.hit) {
                mvwaddstr(action_window, bonus.pos_y, bonus.pos_x, shape_dead);
                bonus.hit = false;
                bonus_active = false;
                if (b_direction) {
                    b_direction = 0;
                    bonus.pos_x = max_x - 4;
                } else {
                    b_direction = 1;
                    bonus.pos_x = 1;
                }
                score += 100;
                switch (iter % 3) {
                case 0:
                    bullets_cnt += 50;
                    break;
                case 1:
                    player.health++;
                    break;
                case 2:
                    gain++;
                    break;
                }
            } else {
                mvwaddstr(action_window, bonus.pos_y, bonus.pos_x, shape_bonus);
                if (iter % (speed + 1) == speed) {
                    (b_direction) ? bonus.pos_x++ : bonus.pos_x--;
                    if (!b_direction && bonus.pos_x == 1) {
                        bonus_active = false;
                        b_direction = 1;
                    } else if (b_direction && (bonus.pos_x == max_x - 4)) {
                        bonus_active = false;
                        b_direction = 0;
                    }
                }
            }
        }

        for (size_t n = 0; n < en_bullets.n_memb; n++) {
            nm_si_t *b = nm_vect_at(&en_bullets, n);
            mvwaddch(action_window, b->pos_y, b->pos_x, '|');

            if (((b->pos_x >= player.pos_x && b->pos_x <= player.pos_x + 2)) &&
                    (player.pos_y == b->pos_y)) {
                player.health--;
                player.hit = true;
            }

            b->pos_y++;

            if (b->pos_y == max_y - 1) {
                nm_vect_delete(&en_bullets, n, NULL);
                n--;
            }
        }

        if (player.health) {
            if (player.hit) {
                mvwaddstr(action_window, player.pos_y, player.pos_x, shape_hit);
                player.hit = false;
            } else {
                mvwaddstr(action_window, player.pos_y, player.pos_x, shape_player);
            }
        } else {
            mvwaddstr(action_window, player.pos_y, player.pos_x, shape_dead);
            play = false;
        }

        wrefresh(action_window);

        usleep(NM_SI_DELAY);
        iter++;
        if (gain_active) {
            gain_counter--;
            if (!gain_counter) {
                gain_active = false;
            }
        }
    }

    nodelay(action_window, FALSE);
    NM_ERASE_TITLE(action, getmaxx(action_window));
    nm_str_format(&info, "%s [score: %zu hp: %d ammo: %zu ap: %zu]",
            next ? "(n)ext level, (q)uit" : "Game over, (q)uit, (r)eplay",
            score, player.health, bullets_cnt, gain);
    nm_init_action(info.data);

    nm_str_free(&info);
    nm_vect_free(&pl_bullets, NULL);
    nm_vect_free(&en_bullets, NULL);
    nm_vect_free(&enemies, NULL);

    do {
        mch = wgetch(action_window);
    } while ((mch != 'n') && (mch != 'q') && (mch != 'r'));

    if (mch == 'n' && next) {
        nm_si_game();
    } else if (mch == 'r') {
        si_player_init = false;
        nm_si_game();
    } else {
        si_player_init = false;
    }
}

void nm_print_nemu(void)
{
    int ch;
    size_t max_y = getmaxy(action_window);
    const char *nemu[] = {
        "            .x@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@o.           ",
        "          .x@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@x.          ",
        "         o@@@@@@@@@@@@@x@@@@@@@@@@@@@@@@@@@@@@@@@o         ",
        "       .x@@@@@@@@x@@@@xx@@@@@@@@@@@@@@@@@@@@@@@@@@o        ",
        "      .x@@@@@@@x. .x@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@.       ",
        "      x@@@@@ox@x..o@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@.      ",
        "     o@@@@@@xx@@@@@@@@@@@@@@@@@@@@@@@@x@@@@@@@@@@@@@o      ",
        "    .x@@@@@@@@@@x@@@@@@@@@@@@@@@@@@@@@o@@@@@@@@@@@@@@.     ",
        "    .@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@x.@@@@@@@@@@@@@@o     ",
        "    o@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@xx..o@@@@@@@@@@@@@x     ",
        "    x@@@@@@@@@@@@@@@@@@xxoooooooo........oxxx@@@@@@@@x.    ",
        "    x@@@@@@@@@@x@@@@@@x. .oxxx@@xo.       .oxo.o@@@@@x     ",
        "    o@@@@@@@@x..o@@@@@o .xxx@@@o ..      .x@ooox@@@@@o     ",
        "    .x@@@@@@@oo..x@@@@.  ...oo..         .oo. o@@@@@o.     ",
        "     .x@@@@@@xoooo@@@@.                       x@@@x..      ",
        "       o@@@@@@ooxx@@@@.                 .    .@@@x..       ",
        "        .o@@@@@@o.x@@@.                 ..   o@@x.         ",
        "         .x@@@@@@xx@@@.                 ..  .x@o           ",
        "         o@@@@@@@o.x@@.                    .x@x.           ",
        "         o@@@@@@@o o@@@xo.         .....  .x@x.            ",
        "          .@@@@@x. .x@o.x@x..           .o@@@.             ",
        "         .x@@@@@o.. o@o  .o@@xoo..    .o..@@o              ",
        "          o@@@@oo@@xx@x.   .x@@@ooooooo. o@x.              ",
        "         .o@@o..xx@@@@@xo.. o@x.         xx.               ",
        "          .oo. ....ox@@@@@@xx@o          xo.               ",
        "          ....       .o@@@@@@@@o        .@..               ",
        "           ...         ox.ox@@@.        .x..               ",
        "            ...        .x. .@@x.        .o                 ",
        "              .         .o .@@o.         o                 ",
        "                         o. x@@o         .                 ",
        "                         .o o@@..        .                 ",
        "                          ...@o..        .                 ",
        "                           .x@xo.                          ",
        "                            .x..                           ",
        "                           .x....                          ",
        "                           o@..xo                          ",
        "                           o@o oo                          ",
        "                           ..                              "
    };

    for (size_t l = 3, n = 0; n < (max_y - 4) && n < nm_arr_len(nemu); n++, l++)
        mvwprintw(action_window, l, 1, "%s", nemu[n]);

    ch = wgetch(action_window);
    if (ch == NM_KEY_S) {
        nm_si_game();
    }
}

/* vim:set ts=4 sw=4: */
