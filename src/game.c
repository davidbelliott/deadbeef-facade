#include "list.h"
#include "common.h"

#include <whitgl/sound.h>
#include <whitgl/input.h>
#include <whitgl/logging.h>
#include <whitgl/math.h>
#include <whitgl/sys.h>
#include <whitgl/random.h>
#include <whitgl/timer.h>

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>


static unsigned int map[MAP_WIDTH][MAP_HEIGHT] =
{
	{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,2,2,2,2,2,0,0,0,0,3,0,3,0,3,0,0,0,1},
	{1,0,0,0,0,0,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,2,0,0,0,2,0,0,0,0,3,0,0,0,3,0,0,0,1},
	{1,0,0,0,0,0,2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,2,2,0,2,2,0,0,0,0,3,0,3,0,3,0,0,0,1},
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,4,0,4,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,4,0,0,0,0,5,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,4,0,4,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,4,0,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,4,4,4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
	{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

static unsigned int drawn[MAP_WIDTH][MAP_HEIGHT] = {{ 0 }};

typedef struct physics_obj {
    unsigned int id;
    point2 pos;
    vector2 vel;
    double radius;
} physics_obj;

typedef struct astar_node_t {
    ipoint2 pt;
    double f, g, h;
    struct astar_node_t *from;
} astar_node_t;

typedef struct anim_obj {
    unsigned int id;
    whitgl_sprite sprite;
    whitgl_ivec frametr;
    int n_frames[MAX_N_ANIM_STATES];
    int delay;  // delay between frames in number of beats (min 1)
    bool loop;
} anim_obj;

typedef struct player_t {
    physics_obj *phys;
    double angle;
    struct vector2 look_direction;
    struct vector2 move_direction;
    int health;
    float time_since_damaged;
    int targeted_rat;
} player_t;

typedef struct note_pop_text_t {
    bool exists;
    float life;
    char text[SHORT_TEXT_MAX_LEN];
} note_pop_text_t;

enum {
    RAT_TYPE_BASIC,
    RAT_TYPE_SPRINTING,
    RAT_TYPE_TURRET,
    RAT_TYPE_REPRODUCER
};

typedef struct note_t {
    bool exists;
    char type;
    bool popped;
    bool hold;
    whitgl_ivec pos;
    int channel;
    anim_obj *anim;
} note_t;

typedef struct rat_t {
    unsigned int id;
    int type;
    physics_obj *phys;
    anim_obj *anim;
    list_t *path;
    int health;
    bool dead;
    note_t beat[NOTES_PER_MEASURE * MEASURES_PER_LOOP];
} rat_t;

typedef struct loop_t {
    char name[SHORT_TEXT_MAX_LEN];
    note_t notes[TOTAL_NUM_NOTES];
} loop_t;

loop_t aloops[NUM_LOOPS] = {{{ 0 }}};
int earliest_active_note_offset = 0;
int cur_loop = 0;
int cur_note = 0;
whitgl_ivec note_pop_pos = {SCREEN_W / 2 - 8, SCREEN_H / 2 - 8};

note_pop_text_t note_pop_text = { 0 };

anim_obj *anim_objs[MAX_N_ANIM_OBJS] = {0};

physics_obj* phys_objs[MAX_N_PHYS] = {0};
unsigned int n_phys_objs = 0;

player_t* player = NULL;

rat_t* rats[MAX_N_RATS] = {0};



int cycles_to_astar = 0;
int actual_song_millis = 0;
int prev_actual_song_millis = 0;

whitgl_float time_since_note = 0;
whitgl_float song_time = 0.0f;
whitgl_float song_len = 0.0f;

// Definitions of static helper functions
static int get_closest_targeted_rat(point2 player_pos, vector2 player_look);

static anim_obj *anim_create(whitgl_sprite sprite, whitgl_ivec frametr, int n_frames[MAX_N_ANIM_STATES], int delay, bool loop) {
    int i = 0;
    for (i = 0; i < MAX_N_ANIM_OBJS; i++) {
        if (!anim_objs[i])
            break;
    }
    anim_obj *anim = NULL;
    if (i < MAX_N_ANIM_OBJS) {
        anim = (anim_obj*)malloc(sizeof(anim_obj));
        anim->id = i;
        anim->sprite = sprite;
        anim->frametr = frametr;
        memcpy(anim->n_frames, n_frames, MAX_N_ANIM_STATES * sizeof(int));
        anim->delay = MAX(1, delay);
        anim->loop = loop;
        anim_objs[i] = anim;
    } else {
        WHITGL_LOG("Error: out of animations");
    }
    return anim;
}

static int get_targeted_rat(int cur_targeted, point2 player_pos, vector2 player_look) {
    int next_targeted = get_closest_targeted_rat(player_pos, player_look);
    return next_targeted;
}

static void anim_destroy(anim_obj *anim) {
    anim_objs[anim->id] = NULL;
    free(anim);
}

void apply_anim_to_model(char *model_data, int n_verts, anim_obj *anim) {
    float *data = (float*)model_data;
    for (int i = 0; i < n_verts; i++) {
        int idx = 11 * i + 3;   // stride: 11, texturepos offset: 3
        data[idx] += anim->sprite.size.x * anim->frametr.x;
        data[idx + 1] += anim->sprite.size.y * anim->frametr.y;
    }
}

whitgl_ivec get_note_pos(int channel, int offset_idx, int earliest_idx, int latest_idx) {
    int total_path_len = note_pop_pos.x + 8 + note_pop_pos.y - 8;
    whitgl_ivec pos = {0, 0};
    float note_completion = NOTES_PER_MEASURE * BPM / 60.0f / 4.0f * time_since_note;
    float completion = ((float)(earliest_idx - offset_idx) + note_completion) / ((float)(earliest_idx));
    float ease_completion = completion >= 1.0f ? 1.0f : 1.0f - (completion - 1.0f) * (completion - 1.0f);
    //int x_total_offset = (int)((float)SCREEN_W * (channel + 1.0f) / 5.0f) - note_pop_pos.x;
    int x_total_offset = 0;
    int y_total_offset = -note_pop_pos.y - 16;
    /*int path_pos = (int)(completion * total_path_len);
    if (path_pos < note_pop_pos.x + 8) {
        pos.x = SCREEN_W + 8 - path_pos;
        pos.y = 8;
    } else {
        pos.x = note_pop_pos.x;
        pos.y = path_pos - (note_pop_pos.x + 8) + 8;
    }*/
    pos.x = x_total_offset * (1.0f - completion) + note_pop_pos.x;
    pos.y = y_total_offset * (1.0f - completion) + note_pop_pos.y;
    return pos;
}

static void note_create(note_t *note, int exists) {
    if (exists) {
        whitgl_sprite note_sprite = {0, {128,64},{16,16}};
        whitgl_ivec frametr = {0, 0};
        int n_frames[MAX_N_ANIM_STATES] = {1, 4};
        anim_obj *anim = anim_create(note_sprite, frametr, n_frames, NOTES_PER_MEASURE / 16, false);
        note->exists = true;
        note->type = 0;
        note->popped = false;
        note->hold = false;
        note->pos.x = 0;
        note->pos.y = 0;
        note->channel = rand() % 4;
        note->anim = anim;
    } else {
        note->exists = false;
    }
}

void load_loops(char *file) {
    for (int i = 0; i < NUM_LOOPS; i++) {
        for (int j = 0; j < NOTES_PER_MEASURE * MEASURES_PER_LOOP; j++) {
            if (!(j % (96/(4*(i + 1))))) {// && !(rand() % 2)) {
                whitgl_sprite note_sprite = {0, {128,64},{16,16}};
                whitgl_ivec frametr = {0, 0};
                int n_frames[MAX_N_ANIM_STATES] = {1, 4};
                anim_obj *anim = anim_create(note_sprite, frametr, n_frames, NOTES_PER_MEASURE / 16, false);
                note_t note = {true, 0, false, false, 0, 0, rand() % 4, anim};
                aloops[i].notes[j] = note;
            } else {
                aloops[i].notes[j].exists = false;
            }
        }
    }
    int i = 0;
    strcpy(aloops[i++].name, "Big Chune 1");
    strcpy(aloops[i++].name, "Dragon Tor");
    strcpy(aloops[i++].name, "Amen");
    strcpy(aloops[i++].name, "Fashy Chune");
}

void destr_loops() {
    for (int i = 0; i < NUM_LOOPS; i++) {
        for (int j = 0; j < NOTES_PER_MEASURE * MEASURES_PER_LOOP; j++) {
            if (aloops[i].notes[j].exists) {
                anim_destroy(aloops[i].notes[j].anim);
            }
            aloops[i].notes[j].exists = false;
        }
    }
}

static physics_obj* phys_create(point2 *pos, vector2 *vel, double radius) {
    physics_obj *ptr = NULL;
    if(n_phys_objs < MAX_N_PHYS) {
        ptr = (physics_obj*)malloc(sizeof(physics_obj));
        ptr->id = n_phys_objs++;
        ptr->pos = *pos;
        ptr->vel = *vel;
        ptr->radius = radius;
        phys_objs[ptr->id] = ptr;
    }
    return ptr;
}

static void phys_destroy(physics_obj *phys) {
    phys_objs[phys->id] = NULL;
    free(phys);
}

static void player_destroy() {
    phys_destroy(player->phys);
    free(player);
    player = NULL;
}

static void player_create(point2 *pos, double angle) {
    if(player)
        player_destroy();
    player = (player_t*)malloc(sizeof(player_t));
    vector2 zero = {0.0, 0.0};
    player->phys = phys_create(pos, &zero, 0.2);
    player->angle = angle;
    player->look_direction = zero;
    player->move_direction = zero;
    player->health = 100;
    player->time_since_damaged = 0.0f;
    player->targeted_rat = -1;
}

static rat_t* rat_create(point2 *pos) {
    rat_t *rat = NULL;
    for (int i = 0; i < MAX_N_RATS; i++) {
        if(!rats[i]) {
            rat = (rat_t*)malloc(sizeof(rat_t));
            rat->id = i;
            vector2 vel = {0.0, 0.0};
            rat->phys = phys_create(pos, &vel, 0.3);
            whitgl_sprite rat_sprite = {0, {128,0}, {64,64}};
            whitgl_ivec frametr = {0, 0};
            int n_frames[MAX_N_ANIM_STATES] = {2};
            rat->anim = anim_create(rat_sprite, frametr, n_frames, NOTES_PER_MEASURE / 4, true);
            rat->path = NULL;
            rat->health = 10;
            rat->dead = false;

            for (int j = 0; j < NOTES_PER_MEASURE * MEASURES_PER_LOOP; j++) {
                note_create(&rat->beat[j], rand() % 16 == 0);
            }
            rats[rat->id] = rat;
            break;
        }
    }
    return rat;
}

void set_note_pop_text(char text[SHORT_TEXT_MAX_LEN]) {
    note_pop_text.exists = true;
    note_pop_text.life = 60.0f / BPM;
    strncpy(note_pop_text.text, text, SHORT_TEXT_MAX_LEN);
}

static void rat_destroy(rat_t *rat) {
    phys_destroy(rat->phys);
    rat->phys = NULL;
    rats[rat->id] = NULL;
    free(rat);
}

static void set_vel(physics_obj *phys, vector2 *dir, double speed) {
    double normalization_const = sqrt(pow(dir->x, 2) + pow(dir->y, 2));
    normalization_const = (normalization_const == 0.0 ? 0.0 : 1.0 / normalization_const);
    phys->vel.x = dir->x * normalization_const * speed;
    phys->vel.y = dir->y * normalization_const * speed;
}

static bool try_to_move(physics_obj *obj, struct point2 *pos)
{
	int x0, x1, y0, y1;
	double size = obj->radius;

	x0 = pos->x - size;
	x1 = pos->x + size;
	y0 = pos->y - size;
	y1 = pos->y + size;

	for (int x = x0; x <= x1; x++) {
		for (int y = y0; y <= y1; y++) {
			if (map[x][y])
				return false;
		}
	}
        /*for (int i = 0; i < n_phys_objs; i++) {
            if(i != obj->id && pow(phys_objs[i]->pos.x - pos->x, 2) + pow(phys_objs[i]->pos.y - pos->y, 2)
                    < pow(phys_objs[i]->radius + obj->radius, 2)) {
                return false;
            }
        }*/
	return true;
}

static void clip_move(physics_obj *phys, point2 *new_pos) {
    struct point2 pos;
    struct point2 old_pos = phys->pos;

    pos.x = new_pos->x;
    pos.y = new_pos->y;
    if(try_to_move(phys, &pos)) {
        phys->pos = pos;
        return;
    }

    pos.x = new_pos->x;
    pos.y = old_pos.y;
    if(try_to_move(phys, &pos)) {
        phys->pos = pos;
        return;
    }

    pos.x = old_pos.x;
    pos.y = new_pos->y;
    if(try_to_move(phys, &pos)) {
        phys->pos = pos;
        return;
    }

    phys->pos = old_pos;
}


static void phys_obj_update(physics_obj *phys, float dt) {
    //printf("Moving with vel (%f, %f)\n", phys->vel.x, phys->vel.y);
    struct point2 new_pos;
    new_pos.x = phys->pos.x + phys->vel.x * dt;
    new_pos.y = phys->pos.y + phys->vel.y * dt;
    clip_move(phys, &new_pos);
}


static double degrees_to_radians(double angle) {
	return angle * M_PI/180.0;
}

static bool eq(ipoint2 p1, ipoint2 p2) {
    return p1.x == p2.x && p1.y == p2.y;
}

typedef void (*draw_wall_fn)(whitgl_fvec3 pos, whitgl_fmat view, whitgl_fmat persp);


// The VBO containing the 4 vertices of the particles.
static struct point2 square_mesh[] = {
    {-0.5,  0.0},
    { 0.5,  0.0},
    { 0.5,  1.0},
    {-0.5,  1.0}
};

whitgl_fmat make_billboard_mat(whitgl_fmat BM, whitgl_fmat const MV)
{
    whitgl_fmat ret = BM;
    for(size_t i = 0; i < 3; i++) {
        for(size_t j = 0; j < 3; j++) {
            ret.mat[i*4+j] = i==j ? 1 : 0;
        }
        ret.mat[i*4+3] = MV.mat[i*4+3];
    }

    for(size_t i = 0; i < 4; i++) {
        ret.mat[3*4+i] = MV.mat[3*4+i];
    }
    return ret;
}


astar_node_t* astar_alloc_node(int x, int y, astar_node_t *from) {
    astar_node_t *ptr = (astar_node_t*)malloc(sizeof(astar_node_t));
    ptr->pt.x = x;
    ptr->pt.y = y;
    ptr->f = 0;
    ptr->g = 0;
    ptr->h = 0;
    ptr->from = from;
    return ptr;
}

astar_node_t* astar_alloc_copy(astar_node_t *node) {
    astar_node_t *ptr = (astar_node_t*)malloc(sizeof(astar_node_t));
    memcpy(ptr, node, sizeof(astar_node_t));
    return ptr;
}

/*void print_list(node_t **head) {
    node_t *s = *head;
    while(s) {
        printf("(%d, %d)", s->pt.x, s->pt.y);
        s = s->next;
        if(s)
            printf("->");
    }
    printf("\n");
}

void print_path(node_t *end) {
    node_t *from = end;
    while(from) {
        printf("(%d, %d)<-", from->pt.x, from->pt.y);
        from = from->from;
    }
    printf("\n");
}*/

void astar_insert_sort_f(list_t **head, astar_node_t *node) {
    list_t *ibef = *head;
    if(!ibef) { //empty list: create a new linked list
        push_front(head, node);
    } else {
        bool inserted = false;
        for(; ibef->next; ibef=ibef->next) {
            if(((astar_node_t*)ibef->data)->f > node->f) {
                insert_before(head, ibef, node);
                inserted = true;
                break;
            }
        }
        if(!inserted)
            insert_after(head, ibef, (void*)node);
    }
}

bool astar_in(list_t **head, astar_node_t *node) {
    bool in = false;
    list_t *q = *head;
    while(q) {
        if(eq(((astar_node_t*)q->data)->pt, node->pt)) {
            return true;
        }
        q = q->next;
    }
    return false;
}

double idiag_distance(ipoint2 start, ipoint2 end) {
    int max = MAX(abs(start.x - end.x), abs(start.y - end.y));
    int min = MIN(abs(start.x - end.x), abs(start.y - end.y));
    return (double)(max - min) + SQRT2 * (double)min;
}

double diag_distance(point2 start, point2 end) {
    double max = MAX(abs(start.x - end.x), abs(start.y - end.y));
    double min = MIN(abs(start.x - end.x), abs(start.y - end.y));
    return (max - min) + SQRT2 * min;
}

list_t* reconstruct_path(astar_node_t *q) {
    list_t *path = NULL;
    while(q) {
        push_front(&path, astar_alloc_copy(q));
        q = q->from;
    }
    return path;
}

void astar_free_node_list(list_t **q) {
    while (*q) {
        astar_node_t *data = (astar_node_t*)pop_front(q);
        free(data);
    }
}

list_t* astar(ipoint2 start, ipoint2 end) {
    static const int offsets[] = {-1, 0, 1};
    list_t *open = NULL;
    list_t *closed = NULL;
    push_front(&open, astar_alloc_node(start.x, start.y, NULL));

    while(open) {
        astar_node_t *q = (astar_node_t*)pop_front(&open);
        astar_insert_sort_f(&closed, q);
        list_t *s = NULL;
        int x = q->pt.x;
        int y = q->pt.y;
        if(eq(q->pt, end)) {  // goal reached
            list_t *path = reconstruct_path(q);
            astar_free_node_list(&open);
            astar_free_node_list(&closed);
            return path;
        }
        for(int i = 0; i < 3; i++) {
            int dx = offsets[i];
            for(int j = 0; j < 3; j++) {
                int dy = offsets[j];
                if(     (dx != 0 || dy != 0) // not both zero
                        && x+dx < MAP_WIDTH && x+dx >= 0 && y+dy < MAP_HEIGHT && y+dy >= 0 // within bounds
                        && map[x+dx][y+dy] == 0 // no wall in target spot
                        && (dx == 0 || dy == 0 || (map[x][y+dy] == 0 && map[x+dx][y] == 0))) // valid if diagonal
                {
                    /*if(q->pt.x == 9 && q->pt.y == 5)
                        printf("Found neighbor: (%d, %d)\n", x+dx, y+dy);*/
                    push_front(&s, astar_alloc_node(x+dx, y+dy, q));
                }
            }
        }
        while(s) {
            astar_node_t *s_node = (astar_node_t*)s->data;
            s_node->g = q->g + idiag_distance(s_node->pt, q->pt);
            s_node->h = idiag_distance(s_node->pt, end);
            s_node->f = s_node->g + s_node->h;
            bool add = true;
            add = !astar_in(&closed, s_node);
            list_t *check = open;
            while(add && check) {
                astar_node_t *c_node = (astar_node_t*)check->data;
                if(eq(s_node->pt, c_node->pt) && s_node->g >= c_node->g) {
                    add = false;
                    break;
                }
                check = check->next;
            }
            if(add) {
                astar_insert_sort_f(&open, astar_alloc_copy(s_node));
            }
            s = s->next;
        }
        astar_free_node_list(&s);
    }
    //pathfinding failed; return a path to the closest point
    list_t *q = pop_front(&closed);
    list_t *path = reconstruct_path(q->data);
    free(q);
    astar_free_node_list(&open);
    astar_free_node_list(&closed);
    return path;
}

static void draw_skybox(whitgl_fvec3 pos, whitgl_fmat view, whitgl_fmat persp) {
    whitgl_fmat model_matrix = whitgl_fmat_translate(pos);
    whitgl_sys_draw_model(3, WHITGL_SHADER_EXTRA_0, model_matrix, view, persp);
}

static void draw_billboard(whitgl_fvec3 pos, whitgl_fmat view, whitgl_fmat persp) {
    whitgl_fmat model_matrix = whitgl_fmat_translate(pos);
    whitgl_sys_draw_model(2, WHITGL_SHADER_EXTRA_1, model_matrix, view, persp);
}
    
static void draw_wall(whitgl_fvec3 pos, whitgl_fmat view, whitgl_fmat persp) {
    whitgl_fvec wall_offset = {0, 0};
    whitgl_fvec wall_size = {64, 64};
    whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_0, 2, wall_offset);
    whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_0, 3, wall_size);
    whitgl_fmat model_matrix = whitgl_fmat_translate(pos);
    whitgl_sys_draw_model(0, WHITGL_SHADER_EXTRA_0, model_matrix, view, persp);
}

static void draw_floor(whitgl_fvec3 pos, whitgl_fmat view, whitgl_fmat persp) {
    whitgl_fvec floor_offset = {64, 0};
    whitgl_fvec floor_size = {64, 64};
    whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_0, 2, floor_offset);
    whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_0, 3, floor_size);
    whitgl_fmat model_matrix = whitgl_fmat_translate(pos);
    whitgl_sys_draw_model(1, WHITGL_SHADER_EXTRA_0, model_matrix, view, persp);
}

static void draw_window(char *title, whitgl_iaabb iaabb, whitgl_sys_color fill) {
    whitgl_sys_draw_iaabb(iaabb, fill);
    whitgl_sprite font = {0, {0,64}, {FONT_CHAR_W,FONT_CHAR_H}};
    whitgl_ivec text_pos = {iaabb.a.x, iaabb.a.y};
    whitgl_sys_draw_text(font, title, text_pos);
}

static void draw_overlay(int cur_loop, int cur_note) {
    whitgl_iaabb iaabb = {{0, SCREEN_H - FONT_CHAR_H}, {SCREEN_W * player->health / 100, SCREEN_H}};
    whitgl_sys_color col = {0, 0, 255, 255};
    char health[256];
    snprintf(health, 256, "HEALTH: %d", player->health);
    draw_window(health, iaabb, col);

    /*whitgl_sys_color fill = {0, 0, 0, 255};
    whitgl_sys_color border = {255, 255, 255, 255};
    whitgl_iaabb zune_iaabb = {{8, 8}, {126, SCREEN_H - 8}};
    draw_window("My Zune", zune_iaabb, fill, border);

    int first_loop_shown = MIN(MAX(0, cur_loop - 2), NUM_LOOPS - 1 - 5);
    for (int i = 0; i < 4; i++) {
        int loop_idx = i + first_loop_shown;
        whitgl_sys_color fill = {0, 0, 0, 0};
        whitgl_sys_color outline = {0, 0, 0, 0};
        if (loop_idx == cur_loop || !loop_active) {
            int most_recent_note = 0;
            int total_notes = NOTES_PER_MEASURE * MEASURES_PER_LOOP;
            for ( ; most_recent_note < 16; most_recent_note ++) {
                int note_idx = _mod(cur_note - most_recent_note, total_notes);
                if (aloops[loop_idx].notes[note_idx].exists) {
                    break;
                }
            }
            whitgl_sys_color fill_active = {0, 0, (most_recent_note < 8 ? 255 * (8 - most_recent_note) / 8 : 0), 255};
            whitgl_sys_color outline_active = {0, 255, 255, 255};
            fill = fill_active;
            outline = outline_active;
        }
        whitgl_iaabb loop_iaabb = {{zune_iaabb.a.x, zune_iaabb.a.y + (FONT_CHAR_H + 9) * (i + 1)},
                                    {zune_iaabb.b.x, zune_iaabb.a.y + (FONT_CHAR_H + 9) * (i + 1) + FONT_CHAR_H + 9}};
        draw_window(aloops[loop_idx].name, loop_iaabb, fill, outline);
    }*/
}

static void draw_notes(note_t *beat, int cur_note, float time_since_note, int x) {
    int total_notes = NOTES_PER_MEASURE * MEASURES_PER_LOOP;
    int earliest_idx = NOTES_PER_MEASURE / 2;
    int latest_idx = MAX(-NOTES_PER_MEASURE / 8, earliest_active_note_offset);
    for (int i = latest_idx; i < earliest_idx; i++) {
        int note_idx = _mod(cur_note + i, total_notes);
        note_t *note = &beat[note_idx];
        if (note->exists) {
            if (!note->popped) {
                whitgl_ivec pos = get_note_pos(note->channel, i, earliest_idx, latest_idx);
                pos.x = x;
                note->pos = pos;
            }
            anim_obj *anim = note->anim;
            //whitgl_sys_draw_sprite(anim->sprite, anim->frametr, note->pos);
            whitgl_iaabb note_iaabb = {note->pos, {note->pos.x + 16, note->pos.y + 16}};
            whitgl_sys_color fill = {0, 0, 255, 255};
            whitgl_sys_color outline = {255, 255, 255, 255};
            whitgl_sys_draw_iaabb(note_iaabb, fill);
            whitgl_sys_draw_hollow_iaabb(note_iaabb, 1, outline);
        }
    }
}

static whitgl_ivec point_project(whitgl_fvec3 pos, whitgl_fmat mv, whitgl_fmat projection) {
    float objx = pos.x;
    float objy = pos.y;
    float objz = pos.z;
    // Transformation vectors
    float fTempo[8];

    whitgl_ivec window_coord = {0, 0};
    // Modelview transform
    fTempo[0]=mv.mat[0]*objx+mv.mat[4]*objy+mv.mat[8]*objz+mv.mat[12]; // w is always 1
    fTempo[1]=mv.mat[1]*objx+mv.mat[5]*objy+mv.mat[9]*objz+mv.mat[13];
    fTempo[2]=mv.mat[2]*objx+mv.mat[6]*objy+mv.mat[10]*objz+mv.mat[14];
    fTempo[3]=mv.mat[3]*objx+mv.mat[7]*objy+mv.mat[11]*objz+mv.mat[15];
    // Projection transform, the final row of projection matrix is always [0 0 -1 0]
    // so we optimize for that.
    fTempo[4]=projection.mat[0]*fTempo[0]+projection.mat[4]*fTempo[1]+projection.mat[8]*fTempo[2]+projection.mat[12]*fTempo[3];
    fTempo[5]=projection.mat[1]*fTempo[0]+projection.mat[5]*fTempo[1]+projection.mat[9]*fTempo[2]+projection.mat[13]*fTempo[3];
    fTempo[6]=projection.mat[2]*fTempo[0]+projection.mat[6]*fTempo[1]+projection.mat[10]*fTempo[2]+projection.mat[14]*fTempo[3];
    fTempo[7]=-fTempo[2];
    // The result normalizes between -1 and 1
    if(fTempo[7]==0.0) // The w value
        return window_coord;
    fTempo[7]=1.0/fTempo[7];
    // Perspective division
    fTempo[4]*=fTempo[7];
    fTempo[5]*=fTempo[7];
    fTempo[6]*=fTempo[7];
    // Window coordinates
    // Map x, y to range 0-1
    window_coord.x=(fTempo[4]*0.5+0.5)*SCREEN_W;
    window_coord.y=(fTempo[5]*0.5+0.5)*SCREEN_H;
    // This is only correct when glDepthRange(0.0, 1.0)
    //windowCoordinate[2]=(1.0+fTempo[6])*0.5;	// Between 0 and 1
    return window_coord;
}

static void draw_rat_overlays(int targeted_rat, int cur_note, float time_since_note, whitgl_fmat view, whitgl_fmat persp) {
    if (targeted_rat != -1) {
        note_t *beat = rats[targeted_rat]->beat;
        whitgl_fvec3 model_pos = {0.0f, 0.0f, 0.0f};
        whitgl_fvec3 pos = {rats[targeted_rat]->phys->pos.x, rats[targeted_rat]->phys->pos.y, 0.5f};
        whitgl_fmat mv = whitgl_fmat_multiply(view, whitgl_fmat_translate(pos));
        whitgl_ivec window_coords = point_project(model_pos, mv, persp);

        printf("%d\t%d\n", window_coords.x, window_coords.y);


        whitgl_sys_color fill = {0, 0, 0, 255};
        whitgl_sys_color border = {255, 255, 255, 255};
        int pixel_x = window_coords.x;
        whitgl_iaabb zune_iaabb = {{pixel_x - 8, 0}, {pixel_x + 8, SCREEN_H / 2 + 8}};
        whitgl_iaabb line1 = {{pixel_x - 7, 0}, {pixel_x - 7, SCREEN_H / 2 - 8}};
        whitgl_iaabb line2 = {{pixel_x + 8, 0}, {pixel_x + 8, SCREEN_H / 2 - 8}};
        whitgl_iaabb line3 = {{pixel_x + 8, SCREEN_H / 2 - 8}, {pixel_x, SCREEN_H / 2}};
        whitgl_iaabb line4 = {{pixel_x - 7, SCREEN_H / 2 - 7}, {pixel_x, SCREEN_H / 2}};
        whitgl_sys_draw_iaabb(zune_iaabb, fill);
        whitgl_sys_draw_line(line1, border);
        whitgl_sys_draw_line(line2, border);
        //whitgl_sys_draw_line(line3, border);
        //whitgl_sys_draw_line(line4, border);
        draw_notes(beat, cur_note, time_since_note, pixel_x - 8);

        whitgl_ivec target_pos = note_pop_pos;
        target_pos.x = pixel_x - 8;
        whitgl_sprite sprite = {0, {128,64+16*2},{16,16}};
        whitgl_ivec frametr = {0, 0};
        whitgl_sys_draw_sprite(sprite, frametr, target_pos);
    }
    whitgl_ivec crosshairs_pos = {SCREEN_W / 2, SCREEN_H / 2};
    whitgl_iaabb box1 = {{crosshairs_pos.x - 1, crosshairs_pos.y - 4}, {crosshairs_pos.x + 1, crosshairs_pos.y + 4}};
    whitgl_iaabb box2 = {{crosshairs_pos.x - 4, crosshairs_pos.y - 1}, {crosshairs_pos.x + 4, crosshairs_pos.y + 1}};
    whitgl_sys_color border = {255, 255, 255, 255};
    whitgl_sys_draw_iaabb(box1, border);
    whitgl_sys_draw_iaabb(box2, border);
}

static void draw_note_pop_text() {
        if (note_pop_text.exists) {
            int str_len = strlen("BASED");
            int x = note_pop_pos.x - str_len * FONT_CHAR_W;
            int y = note_pop_pos.y + 8 + 20.0f * (note_pop_text.life);
            whitgl_iaabb iaabb = {{x, y - 12}, {x + str_len * 6, y}};
            whitgl_sys_color col = {0, 0, 0, 255};
            whitgl_sys_draw_iaabb(iaabb, col);

            whitgl_sprite font = {0, {0,64}, {FONT_CHAR_W,FONT_CHAR_H}};
            whitgl_ivec text_pos = { x, y - 12};
            whitgl_sys_draw_text(font, note_pop_text.text, text_pos);
        }
}
            

static void draw_rats(whitgl_fmat view, whitgl_fmat persp) {
    for (int i = 0; i < MAX_N_RATS; i++) {
        if (rats[i]) {
            whitgl_fvec rat_offset = {128 + 64 * rats[i]->anim->frametr.x, 0};
            whitgl_fvec rat_size = {64, 64};
            whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_1, 2, rat_offset);
            whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_1, 3, rat_size);
            whitgl_fvec3 pos = {rats[i]->phys->pos.x, rats[i]->phys->pos.y, 0.5f};
            draw_billboard(pos, view, persp);
        }
    }
}

static void draw_floors(struct point2 *player_pos, whitgl_fmat view, whitgl_fmat persp) {
    for (int x = (int)MAX(0.0f, player_pos->x - MAX_DIST); x < (int)MIN(MAP_WIDTH, player_pos->x + MAX_DIST); x++) {
        for (int y = (int)MAX(0.0f, player_pos->y - MAX_DIST); y < (int)MIN(MAP_HEIGHT, player_pos->y + MAX_DIST); y++) {
            if (map[x][y] == 0) {
                whitgl_fvec3 pos = {x + 0.5f, y + 0.5f, 1.0f};
                draw_floor(pos, view, persp);
            }
        }
    }
}

static void raycast(struct point2 *position, double angle, draw_wall_fn draw_wall, whitgl_fmat view, whitgl_fmat persp)
{
    int n_drawn = 0;
    double fov = whitgl_pi / 2 * (double)SCREEN_W/(double)SCREEN_H;
    double ray_step = fov / (double) SCREEN_W;

    for (int x = 0; x < MAP_WIDTH; x++) {
        for (int y = 0; y < MAP_HEIGHT; y++) {
            drawn[x][y] = 0;
        }
    }

    for (int i = 0; i < (SCREEN_W); i++) {
        struct vector2 ray_direction;
        double next_x, next_y;
        double step_x, step_y;
        double max_x, max_y;
        double ray_angle;
        double dx, dy;
        int x, y;

        ray_angle = angle + -fov/2.0 + ray_step * i;
        ray_direction.x = cos(ray_angle);
        ray_direction.y = sin(ray_angle);

        x = position->x;
        y = position->y;

        step_x = _sign(ray_direction.x);
        step_y = _sign(ray_direction.y);

        next_x = x + (step_x > 0 ? 1 : 0);
        next_y = y + (step_y > 0 ? 1 : 0);

        max_x = (next_x - position->x) / ray_direction.x;
        max_y = (next_y - position->y) / ray_direction.y;

        if (isnan(max_x))
                max_x = INFINITY;
        if (isnan(max_y))
                max_y = INFINITY;

        dx = step_x / ray_direction.x;
        dy = step_y / ray_direction.y;

        if (isnan(dx))
                dx = INFINITY;
        if (isnan(dy))
                dy = INFINITY;

        bool draw = false;
        for (;;) {
            if (map[x][y] != 0) {
                draw = true;
                break;
            }

            if (max_x < max_y) {
                max_x += dx;
                x += step_x;
            } else {
                if(max_y < MAX_DIST) {
                    max_y += dy;
                    y += step_y;
                } else {
                    break;
                }
            }
        }
        if (draw && !drawn[x][y]) {
            whitgl_fvec3 pos = {x + 0.5f, y + 0.5f, 0.5f};
            draw_wall(pos, view, persp);
            drawn[x][y] = 1;
            n_drawn++;
        }
    }
}


static void frame(player_t *p, int cur_loop, int cur_note)
{
    whitgl_sys_draw_init(0);

    whitgl_sys_enable_depth(true);

    whitgl_float fov = whitgl_pi/2;
    whitgl_fmat perspective = whitgl_fmat_perspective(fov, (float)SCREEN_W/(float)SCREEN_H, 0.1f, 100.0f);
    whitgl_fvec3 up = {0,0,-1};
    whitgl_fvec3 camera_pos = {player->phys->pos.x,player->phys->pos.y,0.5f};
    whitgl_fvec3 camera_to = {camera_pos.x + player->look_direction.x, camera_pos.y + player->look_direction.y,0.5f};
    whitgl_fmat view = whitgl_fmat_lookAt(camera_pos, camera_to, up);
    //whitgl_fmat model_matrix = whitgl_fmat_translate(camera_to);
    //model_matrix = whitgl_fmat_multiply(model_matrix, whitgl_fmat_rot_z(time*3));

    //whitgl_sys_draw_model(0, WHITGL_SHADER_MODEL, model_matrix, view, perspective);
    player->phys->pos.x = camera_pos.x;
    player->phys->pos.y = camera_pos.y;
    whitgl_fvec3 skybox_pos = camera_pos;
    skybox_pos.z = -2.5f;
    //draw_skybox(skybox_pos, view, perspective);
    raycast(&player->phys->pos, p->angle, draw_wall, view, perspective);
    draw_floors(&player->phys->pos, view, perspective);
    draw_rats(view, perspective);

    whitgl_sys_enable_depth(false);

    draw_rat_overlays(player->targeted_rat, cur_note, time_since_note, view, perspective);

    //draw_notes(cur_loop, cur_note, time_since_note);
    //draw_note_pop_text();
    draw_overlay(cur_loop, cur_note);

    whitgl_sys_draw_finish();
}

static bool move_toward_point(physics_obj *obj, point2 *target, double speed, double thresh) {
    if(fabs(target->x - obj->pos.x) <= thresh && fabs(target->y - obj->pos.y) <= thresh) {
        obj->vel.x = 0.0;
        obj->vel.y = 0.0;
        return true;
    }
    vector2 dir;
    dir.x = target->x - obj->pos.x;
    dir.y = target->y - obj->pos.y;
    double norm = sqrt(pow(dir.x, 2) + pow(dir.y, 2));
    norm = (norm == 0.0 ? 0.0 : 1.0 / norm);
    obj->vel.x = dir.x * norm * speed;
    obj->vel.y = dir.y * norm * speed;
    return false;
}

bool line_of_sight_exists(point2 *p1, point2 *p2) {
    struct vector2 ray_direction;
    double next_x, next_y;
    double step_x, step_y;
    double max_x, max_y;
    double ray_angle;
    double dx, dy;
    int x, y;

    ray_direction.x = p2->x - p1->x;
    ray_direction.y = p2->y - p1->y;
    double norm = sqrt(pow(ray_direction.x, 2)+pow(ray_direction.y, 2));
    ray_direction.x /= norm;
    ray_direction.y /= norm;

    x = p1->x;
    y = p1->y;

    step_x = _sign(ray_direction.x);
    step_y = _sign(ray_direction.y);

    next_x = x + (step_x > 0 ? 1 : 0);
    next_y = y + (step_y > 0 ? 1 : 0);

    max_x = (next_x - p1->x) / ray_direction.x;
    max_y = (next_y - p1->y) / ray_direction.y;

    if (isnan(max_x))
            max_x = INFINITY;
    if (isnan(max_y))
            max_y = INFINITY;

    dx = step_x / ray_direction.x;
    dy = step_y / ray_direction.y;

    if (isnan(dx))
            dx = INFINITY;
    if (isnan(dy))
            dy = INFINITY;

    while(x != (int)p2->x || y != (int)p2->y) {
            if (map[x][y] != 0) {
                return false;
            }

            if (max_x < max_y) {
                    max_x += dx;
                    x += step_x;
            } else {
                    if(max_y < MAX_DIST) {
                        max_y += dy;
                        y += step_y;
                    } else {
                        break;
                    }
            }
    }
    return true;
}

bool unobstructed(point2 *p1, point2 *p2, double w) {
    //if(fabs(p1->x - p2->x) < 1.0 && fabs(p1->y - p2->y) < 1.0)
    //   return true;
    double dx = p2->x - p1->x;
    double dy = p2->y - p1->y;
    double norm = sqrt(pow(dx, 2)+pow(dy, 2));
    dx /= norm;
    dy /= norm;
    point2 p11 = {p1->x - dy * w, p1->y + dx * w};
    point2 p21 = {p2->x - dy * w, p2->y + dx * w};
    point2 p12 = {p1->x + dy * w, p1->y - dx * w};
    point2 p22 = {p2->x + dy * w, p2->y - dx * w};
    return line_of_sight_exists(&p11, &p21) && line_of_sight_exists(&p12, &p22);
}

void player_deal_damage(player_t *p, int dmg) {
    p->time_since_damaged = 0.0f;
    p->health = MAX(0, p->health - dmg);
}

void rat_deal_damage(rat_t *rat, int dmg) {
    rat->health = MAX(0, rat->health - dmg);
    if (rat->health == 0) {
        rat->dead = true;
    }
}

void rats_prune() {
    for (int i = 0; i < MAX_N_RATS; i++) {
        if (rats[i] && rats[i]->dead) {
            rat_destroy(rats[i]);
        }
    }
}

static void phys_update(float dt) {
    for(int i = 0; i < n_phys_objs; i++) {
        if (phys_objs[i]) {
            phys_obj_update(phys_objs[i], dt);
        }
    }
}

static bool player_update(player_t *p, float dt)
{
    p->look_direction.x = cos(p->angle);
    p->look_direction.y = sin(p->angle);
    vector2 world_move_dir;
    world_move_dir.x = (p->look_direction.x*p->move_direction.x + p->look_direction.y*p->move_direction.y);
    world_move_dir.y = (p->look_direction.y*p->move_direction.x - p->look_direction.x*p->move_direction.y);
    set_vel(p->phys, &world_move_dir, MOVE_SPEED);
    p->time_since_damaged += dt;
    return p->health != 0;
}

/*static void notes_update(struct player_t *p, int cur_loop, int cur_note) {
    if (loop_active) {
        earliest_active_note_offset = MAX(-NOTES_PER_MEASURE / 8, earliest_active_note_offset - 1);
        int bottom_note_idx = _mod(cur_note - NOTES_PER_MEASURE / 4 - 1, NOTES_PER_MEASURE * MEASURES_PER_LOOP);
        note_t *bottom_note = &aloops[cur_loop].notes[bottom_note_idx];
        if (bottom_note->exists) {
            if (bottom_note->popped) {
                bottom_note->popped = false;
            }
            bottom_note->anim->frametr.x = 0;
            bottom_note->anim->frametr.y = 0;
        }
    }
}*/

static void note_pop_text_update(float dt) {
        if (note_pop_text.exists) {
            note_pop_text.life = MAX(0.0f, note_pop_text.life - dt);
            if (note_pop_text.life == 0.0f) {
                note_pop_text.exists = false;
            }
        }
}

static void anim_objs_update() {
    for (int i = 0; i < MAX_N_ANIM_OBJS; i++) {
        if (anim_objs[i]) {
            int n_frames = anim_objs[i]->n_frames[anim_objs[i]->frametr.y];
            int cur_frame = anim_objs[i]->frametr.x;
            if (n_frames > 1) {
                    cur_frame = anim_objs[i]->loop ? (cur_frame + 1) % n_frames : MIN(cur_frame + 1, n_frames - 1);
            }
            anim_objs[i]->frametr.x = cur_frame;
        }
    }
}

static void rats_update(struct player_t *p, unsigned int dt, int cur_note, bool use_astar) {
    for(int i = 0; i < MAX_N_RATS; i++) {
        if (rats[i]) {
            bool line_of_sight = unobstructed(&rats[i]->phys->pos, &p->phys->pos, rats[i]->phys->radius);
            // Deal damage if close enough
            if (line_of_sight && pow(rats[i]->phys->pos.x - p->phys->pos.x, 2) + pow(rats[i]->phys->pos.y - p->phys->pos.y, 2) <= pow(RAT_ATTACK_RADIUS, 2) && cur_note % RAT_ATTACK_NOTES == 0) {
                p->time_since_damaged = 0.0f;
                player_deal_damage(p, RAT_DAMAGE);
            }
            if(!line_of_sight && use_astar && diag_distance(p->phys->pos, rats[i]->phys->pos) < 20.0) {
                ipoint2 start = {(int)(rats[i]->phys->pos.x), (int)(rats[i]->phys->pos.y)};
                ipoint2 end = {(int)(p->phys->pos.x), (int)(p->phys->pos.y)};
                rats[i]->path = astar(start, end);
                astar_node_t *start_node = pop_front(&rats[i]->path);
                free(start_node);
            }
            if(!line_of_sight && rats[i]->path) {
                astar_node_t *next_node = (astar_node_t*)rats[i]->path->data;
                point2 target = {(double)next_node->pt.x + 0.5, (double)next_node->pt.y + 0.5};
                bool reached = move_toward_point(rats[i]->phys, &target, RAT_SPEED, 0.1);
                if(reached) {
                    astar_node_t *reached_node = pop_front(&rats[i]->path);
                    free(reached_node);
                }
            } else if(line_of_sight) {
                rats[i]->path = NULL;
                point2 target;
                target.x = p->phys->pos.x - (p->phys->pos.x - rats[i]->phys->pos.x) * 0.5;
                target.y = p->phys->pos.y - (p->phys->pos.y - rats[i]->phys->pos.y) * 0.5;
                move_toward_point(rats[i]->phys, &target, RAT_SPEED, 0.25);
            }
        }
    }
}

static int get_closest_targeted_rat(point2 player_pos, vector2 player_look) {
    int closest_hit_rat = -1;
    float closest_dist_sq = 0.0f;
    for (int i = 0; i < MAX_N_RATS; i++) {
        if (rats[i]) {
            whitgl_fvec3 d = { rats[i]->phys->pos.x - player_pos.x,
                rats[i]->phys->pos.y - player_pos.y, 0.0f };
            whitgl_fvec3 l = { player_look.x, player_look.y, 0.0f };
            whitgl_fvec3 cross = whitgl_fvec3_cross(l, d);
            float sinsq = (cross.z * cross.z) / (d.x * d.x + d.y * d.y);
            float max_sinsq = RAT_HITBOX_RADIUS * RAT_HITBOX_RADIUS / (RAT_HITBOX_RADIUS * RAT_HITBOX_RADIUS + d.x*d.x + d.y*d.y);
            //WHITGL_LOG("sinsq: %f\tmax_sinsq: %f\tline_of_sight: %d", sinsq, max_sinsq, line_of_sight_exists(&player->phys->pos, &rats[i]->phys->pos));
            if (sinsq < max_sinsq && line_of_sight_exists(&player->phys->pos, &rats[i]->phys->pos)) {
                float dist_sq = d.x * d.x + d.y * d.y;
                if (dist_sq < closest_dist_sq || closest_hit_rat == -1) {
                    closest_hit_rat = i;
                    closest_dist_sq = dist_sq;
                }
            }
        }
    }
    return closest_hit_rat;
}

void game_pause(bool paused) {
    whitgl_loop_set_paused(AMBIENT_MUSIC, paused);
    whitgl_loop_set_paused(0, paused);
}

void game_input()
{
        player_t *p = player;

        vector2 move_dir = {0.0, 0.0};
        if(whitgl_input_held(WHITGL_INPUT_UP))
            move_dir.x += 1.0;
        if(whitgl_input_held(WHITGL_INPUT_DOWN))
            move_dir.x -= 1.0;
        if(whitgl_input_held(WHITGL_INPUT_RIGHT))
            move_dir.y -= 1.0;
        if(whitgl_input_held(WHITGL_INPUT_LEFT))
            move_dir.y += 1.0;
        player->move_direction = move_dir;

        if (whitgl_input_pressed(WHITGL_INPUT_A)) {
            player->targeted_rat = get_targeted_rat(player->targeted_rat, player->phys->pos, player->look_direction);
        }

        /*if (whitgl_input_pressed(WHITGL_INPUT_MOUSE_SCROLL_UP)) {
            change_cur_loop(MIN(cur_loop + 1, NUM_LOOPS - 1), cur_note, time_since_note);
            WHITGL_LOG("scroll up: %d", cur_loop);
        }

        if (whitgl_input_pressed(WHITGL_INPUT_MOUSE_SCROLL_DOWN)) {
            change_cur_loop(MAX(cur_loop - 1, 0), cur_note, time_since_note);
            WHITGL_LOG("scroll down: %d", cur_loop);
        }

        if (whitgl_input_pressed(WHITGL_INPUT_A)) {
            cur_loop_toggle_active(cur_note, time_since_note);
        }*/
        
        /*if (whitgl_input_pressed(WHITGL_INPUT_MOUSE_LEFT) ||
            whitgl_input_pressed(WHITGL_INPUT_MOUSE_RIGHT)) {
            if (loop_active) {
                int best_candidate = NOTES_PER_MEASURE/8+1;
                int best_idx = -1;
                for (int i = MAX(earliest_active_note_offset, -NOTES_PER_MEASURE/8); i < NOTES_PER_MEASURE/8; i++) {
                    int note_idx = _mod(cur_note + i, NOTES_PER_MEASURE * MEASURES_PER_LOOP);
                    if (aloops[cur_loop].notes[note_idx].exists && !aloops[cur_loop].notes[note_idx].popped && abs(i) < best_candidate) {
                        best_idx = note_idx;
                        best_candidate = abs(i);
                    }
                }
                if (best_idx != -1) {
                    float acc = 1.0f - (float)best_candidate / (float)(NOTES_PER_MEASURE / 8);
                    aloops[cur_loop].notes[best_idx].popped = true;
                    aloops[cur_loop].notes[best_idx].anim->frametr.x = 0;
                    aloops[cur_loop].notes[best_idx].anim->frametr.y = 1;
                    if (acc <= 0.25f) {
                        set_note_pop_text("BAD");
                    } else if (acc <= 0.50f) {
                        set_note_pop_text("OK");
                    } else if (acc <= 0.75f) {
                        set_note_pop_text("GOOD");
                    } else {
                        set_note_pop_text("BASED");
                    }
                    int closest_hit_rat = get_closest_targeted_rat(player->phys->pos, player->look_direction);
                    if (closest_hit_rat != -1) {
                        rat_deal_damage(rats[closest_hit_rat], (int)(4.0f * acc));
                    }
                }
            }
        }*/

        whitgl_ivec mouse_pos = whitgl_input_mouse_pos(PIXEL_DIM);
        p->angle = mouse_pos.x * MOUSE_SENSITIVITY / (double)(SCREEN_W);
}

void game_init() {
    load_loops("data/loops.dat");

    whitgl_load_model(0, "data/obj/cube.wmd");
    whitgl_load_model(1, "data/obj/floor.wmd");
    whitgl_load_model(2, "data/obj/billboard.wmd");
    whitgl_load_model(3, "data/obj/skybox.wmd");

    point2 p_pos = {1.5, 2};
    player_create(&p_pos, 0.0);
    for (int x = 0; x < MAP_WIDTH; x++) {
        for (int y = 0; y < MAP_HEIGHT; y++) {
            if (rand() % 50 == 0 && map[x][y] == 0) {
                point2 rat_pos = {x + 0.5f, y + 0.5f};
                rat_create(&rat_pos);
                break;
            }
        }
    }


    cycles_to_astar = 0;
    actual_song_millis = 0;
    prev_actual_song_millis = 0;

    song_len = whitgl_loop_get_length(AMBIENT_MUSIC) / 1000.0f;
}

void game_cleanup() {
    destr_loops();
    player_destroy();
    for (int i = 0; i < MAX_N_RATS; i++) {
        if (!rats[i]) {
            rat_destroy(rats[i]);
        }
    }
}

void game_start() {
    whitgl_loop_seek(AMBIENT_MUSIC, 0.0f);
    whitgl_loop_set_paused(AMBIENT_MUSIC, false);
}

int game_update(float dt) {
    song_time = _fmod(song_time + dt, song_len);
    actual_song_millis = whitgl_loop_tell(AMBIENT_MUSIC);
    if (actual_song_millis != prev_actual_song_millis) {
        float actual_song_time = actual_song_millis / 1000.0f;
        float diff_ff, diff_rr;
        if (actual_song_time > song_time) {
            diff_ff = actual_song_time - song_time;
            diff_rr = song_time + song_len - actual_song_time;
        } else {
            diff_ff = actual_song_time + song_len - actual_song_time;
            diff_rr = song_time - actual_song_time;
        }
        if (diff_ff < diff_rr) {
            song_time += diff_ff / 2.0f;
        } else {
            song_time -= diff_rr / 2.0f;
        }
        song_time = _fmod(song_time, song_len);
        prev_actual_song_millis = actual_song_millis;
    }


    float secs_per_note = (60.0f / BPM * 4 / NOTES_PER_MEASURE);
    int next_cur_note = _mod((int)(song_time / secs_per_note), NOTES_PER_MEASURE * MEASURES_PER_LOOP);
    time_since_note = 0.0f;
    for (; cur_note != next_cur_note; cur_note = _mod(cur_note + 1, NOTES_PER_MEASURE * MEASURES_PER_LOOP)) {
        if (cur_note == 0) {
            whitgl_loop_seek(0, 0.0f);
        }
        //notes_update(player, cur_loop, cur_note);
        if (cur_note % (NOTES_PER_MEASURE / 16) == 0) {
            anim_objs_update();
        }
    }
    note_pop_text_update(dt);
    
    time_since_note = _fmod(song_time, secs_per_note);
    player_update(player, dt);
    if(cycles_to_astar == 0) {
        rats_update(player, dt, cur_note, true);
        cycles_to_astar = 100;
    } else {
        rats_update(player, dt, cur_note, false);
        cycles_to_astar--;
    }
    phys_update(dt);

    rats_prune();

    return 0;
}

void game_frame() {
    frame(player, cur_loop, cur_note);
}
