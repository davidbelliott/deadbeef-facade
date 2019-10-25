#include <SDL_image.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <SDL.h>
#include <stdbool.h>
#include <stddef.h>

#include <whitgl/sound.h>
#include <whitgl/input.h>
#include <whitgl/logging.h>
#include <whitgl/math.h>
#include <whitgl/sys.h>
#include <whitgl/random.h>
#include <whitgl/timer.h>

#include "linmath.h"

void makebillboard_mat4x4(mat4x4 BM, mat4x4 const MV);

static void mat4x4_print(mat4x4 m) {
    int i,j;
    for(i=0; i<4; ++i) {
        for(j=0; j<4; ++j)
            printf("%f, ", m[i][j]);
        printf("\n");
    }
}

const char* vertex_src = "\
#version 150\
\n\
\
in vec3 position;\
in vec2 texturepos;\
in vec3 vertexColor;\
in vec3 vertexNormal;\
out vec2 Texturepos;\
out vec3 fragmentColor;\
out vec3 fragmentNormal;\
out vec3 fragmentPosition;\
out mat4 normalMatrix;\
uniform mat4 m_model;\
uniform mat4 m_view;\
uniform mat4 m_perspective;\
void main()\
{\
	gl_Position = m_perspective * m_view * m_model * vec4( position, 1.0 );\
	Texturepos = texturepos;\
	fragmentColor = vertexColor;\
	fragmentNormal = vertexNormal;\
	fragmentPosition = vec3( m_model * vec4( position, 1.0));\
	normalMatrix = transpose(inverse(m_model));\
}\
";

const char* post_src = "\
#version 150\
\n\
in vec2 Texturepos;\
out vec4 outColor;\
uniform sampler2D tex;\
uniform sampler2D extra;\
uniform float spread;\
void main()\
{\
	vec2 offset = vec2(0.1*spread);\
	outColor = vec4(texture( tex, Texturepos-offset ).r, texture( tex, Texturepos ).g, texture( tex, Texturepos+offset ).ba);\
	outColor = outColor + texture(extra, Texturepos);\
}\
";

enum {
	MAP_WIDTH	= 24,
	MAP_HEIGHT	= 24,
	CELL_SIZE	= 4,
};

enum {
    TEX_WALL = 0,
    TEX_FLOOR = 1,
    TEX_RAT = 2,
    TEX_OVERLAY = 3
};

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define SCREEN_W 320
#define SCREEN_H 240
#define PIXEL_DIM 2
#define MOUSE_SENSITIVITY 2.0
#define MAX_DIST MAX(MAP_WIDTH, MAP_HEIGHT)
#define MOVE_SPEED 3.000
#define RAT_SPEED MOVE_SPEED * 0.6
#define MAX_N_PHYS 1024
#define MAX_N_RATS 256
#define MAX_N_PATHFINDING_STEPS 256
#define CROSSHAIR_DIAM 9
#define SQRT2 1.41421356237

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

typedef struct point2 {
	double x;
	double y;
} point2;

typedef struct ipoint2 {
    int x;
    int y;
} ipoint2;

typedef struct vector2 {
	double x;
	double y;
} vector2;

typedef struct physics_obj {
    unsigned int id;
    point2 pos;
    vector2 vel;
    double radius;
} physics_obj;

typedef struct node_t {
    ipoint2 pt;
    double f, g, h;
    struct node_t *from;
    struct node_t *prev;
    struct node_t *next;
} node_t;

typedef struct player_t {
    physics_obj *phys;
    double angle;
    struct vector2 look_direction;
    struct vector2 move_direction;
} player_t;

typedef struct rat_t {
    unsigned int id;
    physics_obj *phys;
    node_t *path;
} rat_t;

physics_obj* phys_objs[MAX_N_PHYS] = {0};
unsigned int n_phys_objs = 0;

player_t* player = NULL;

rat_t* rats[MAX_N_RATS] = {0};
unsigned int n_rats = 0;

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
}

static rat_t* rat_create(point2 *pos) {
    rat_t *rat = NULL;
    if(n_rats < MAX_N_RATS) {
        rat = (rat_t*)malloc(sizeof(rat_t));
        rat->id = n_rats++;
        vector2 vel = {0.0, 0.0};
        rat->phys = phys_create(pos, &vel, 0.3);
        rat->path = NULL;
        rats[rat->id] = rat;
    }
    return rat;
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

void makebillboard_mat4x4(mat4x4 BM, mat4x4 const MV)
{
    for(size_t i = 0; i < 3; i++) {
        for(size_t j = 0; j < 3; j++) {
            BM[i][j] = i==j ? 1 : 0;
        }
        BM[i][3] = MV[i][3];
    }

    for(size_t i = 0; i < 4; i++) {
        BM[3][i] = MV[3][i];
    }
}


node_t* alloc_node(int x, int y, node_t *from) {
    node_t *ptr = (node_t*)malloc(sizeof(node_t));
    ptr->pt.x = x;
    ptr->pt.y = y;
    ptr->f = 0;
    ptr->g = 0;
    ptr->h = 0;
    ptr->from = from;
    ptr->prev = NULL;
    ptr->next = NULL;
    return ptr;
}

node_t* alloc_copy(node_t *node) {
    node_t *ptr = (node_t*)malloc(sizeof(node_t));
    memcpy(ptr, node, sizeof(node_t));
    return ptr;
}

void free_node_list(node_t **head) {
    node_t *ptr = *head;
    while(ptr) {
        node_t *old_ptr = ptr;
        ptr = ptr->next;
        free(old_ptr);
    }
}

void print_list(node_t **head) {
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
}

bool insert_before(node_t **head, node_t *pos, node_t *node) {
    if(!pos->prev) { // node is new head of existing list
        pos->prev = node;
        node->prev = NULL;
        node->next = pos;
        *head = node;
    } else {
        pos->prev->next = node;
        node->prev = pos->prev;
        node->next = pos;
        pos->prev = node;
    }
}

void insert_after(node_t *pos, node_t *node) {
    //printf("Inserting after\n");
    if(!pos->next) {
        pos->next = node;
        node->prev = pos;
        node->next = NULL;
    } else {
        pos->next->prev = node;
        node->next = pos->next;
        node->prev = pos;
        pos->next = node;
    }
}

void push_front(node_t **head, node_t *node) {
    node->next = *head;
    node->prev = NULL;
    if(*head)
        (*head)->prev = node;
    *head = node;
}

void insert_sort_f(node_t **head, node_t *node) {
    node_t *ibef = *head;
    if(!ibef) { //empty list: create a new linked list
        push_front(head, node);
    } else {
        bool inserted = false;
        for(; ibef->next; ibef=ibef->next) {
            if(ibef->f > node->f) {
                insert_before(head, ibef, node);
                inserted = true;
                break;
            }
        }
        if(!inserted)
            insert_after(ibef, node);
    }
}

bool in(node_t **head, node_t *node) {
    bool in = false;
    node_t *q = *head;
    while(q) {
        if(eq(q->pt, node->pt)) {
            return true;
        }
        q = q->next;
    }
    return false;
}

node_t* pop_front(node_t **head) {
    //printf("Popping front\n");
    node_t *ptr = *head;
    if(ptr) {
        *head = ptr->next;
        if(*head)
            (*head)->prev = NULL;
    }
    return ptr;
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

node_t* reconstruct_path(node_t *q) {
    node_t* path = alloc_copy(q);
    path->next = NULL;
    q = q->from;
    while(q) {
        push_front(&path, alloc_copy(q));
        q = q->from;
    }
    return path;
}

node_t* astar(ipoint2 start, ipoint2 end) {
    static const int offsets[] = {-1, 0, 1};
    node_t *open = alloc_node(start.x, start.y, NULL);
    node_t *closed = NULL;

    while(open) {
        node_t *q = pop_front(&open);
        insert_sort_f(&closed, q);
        node_t *s = NULL;
        int x = q->pt.x;
        int y = q->pt.y;
        if(eq(q->pt, end)) {  // goal reached
            node_t *path = reconstruct_path(q);
            free_node_list(&open);
            free_node_list(&closed);
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
                    push_front(&s, alloc_node(x+dx, y+dy, q));
                }
            }
        }
        while(s) {
            s->g = q->g + idiag_distance(s->pt, q->pt);
            s->h = idiag_distance(s->pt, end);
            s->f = s->g + s->h;
            bool add = true;
            add = !in(&closed, s);
            node_t *node = open;
            while(add && node) {
                if(eq(s->pt, node->pt) && s->g >= node->g) {
                    add = false;
                    break;
                }
                node = node->next;
            }
            if(add) {
                insert_sort_f(&open, alloc_copy(s));
            }
            s = s->next;
        }
        free_node_list(&s);
    }
    //pathfinding failed; return a path to the closest point
    node_t *q = pop_front(&closed);
    node_t *path = reconstruct_path(q);
    free_node_list(&open);
    free_node_list(&closed);
    return path;
}
    
static void draw_wall(whitgl_fvec3 pos, whitgl_fmat view, whitgl_fmat persp) {
    whitgl_fmat model_matrix = whitgl_fmat_translate(pos);
    whitgl_sys_draw_model(0, WHITGL_SHADER_TEXTURE, model_matrix, view, persp);
}

#define FOV 90

static int sign(double x)
{
	if (x < 0) {
		return -1;
	} else if (x > 0) {
		return 1;
	} else {
		return 0;
	}
}

static void raycast(struct point2 *position, double angle, draw_wall_fn draw_wall, whitgl_fmat view, whitgl_fmat persp)
{
    for (int x = 0; x < MAP_WIDTH; x++) {
        for (int y = 0; y < MAP_HEIGHT; y++) {
            if (map[x][y]) {
                whitgl_fvec3 pos = {x + 0.5f, y + 0.5f, 0.5f};
                draw_wall(pos, view, persp);
            }
        }
    }
    return;
	double fov = degrees_to_radians(FOV) * (double)SCREEN_W/(double)SCREEN_H;
	double ray_step = fov / (double) SCREEN_W;

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

		step_x = sign(ray_direction.x);
		step_y = sign(ray_direction.y);

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

		for (;;) {
			if (map[x][y] != 0) {
                                whitgl_fvec3 pos = {x, y, 0.0f};
                                draw_wall(pos, view, persp);
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
	}
}


static void frame(struct player_t *p, float time)
{
	/*mat4x4 project;
	mat4x4_identity(project);
	mat4x4_perspective(project, degrees_to_radians(FOV), (double)SCREEN_W/(double)SCREEN_H, 0.1f, 64.0f);

        mat4x4 model_view;
	mat4x4_identity(model_view);
	vec3 eye;
	eye[0] = p->phys->pos.x;
	eye[1] = 0.5;
	eye[2] = p->phys->pos.y;
	vec3 center;
	center[0] = p->phys->pos.x+p->look_direction.x;
	center[1] = 0.5;
	center[2] = p->phys->pos.y+p->look_direction.y;
	vec3 up;
	up[0] = 0.0;
	up[1] = 1.0;
	up[2] = 0.0;
	mat4x4_look_at(model_view, eye, center, up);

        mat4x4 pmv;
        mat4x4_mul(pmv, project, model_view);

	glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

	draw_floor(pmv);
        //glDisable(GL_DEPTH_TEST);
        //draw_overlay();
        */

        whitgl_sys_draw_init(0);
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
	raycast(&player->phys->pos, p->angle, draw_wall, view, perspective);


        whitgl_iaabb line = {{1,1},{15,15}};
        whitgl_sys_draw_line(line, whitgl_sys_color_white);

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

    step_x = sign(ray_direction.x);
    step_y = sign(ray_direction.y);

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

static void phys_update(float dt) {
    for(int i = 0; i < n_phys_objs; i++) {
        phys_obj_update(phys_objs[i], dt);
    }
}

static void update(struct player_t *p, float dt)
{
    p->look_direction.x = cos(p->angle);
    p->look_direction.y = sin(p->angle);
    vector2 world_move_dir;
    world_move_dir.x = (p->look_direction.x*p->move_direction.x + p->look_direction.y*p->move_direction.y);
    world_move_dir.y = (p->look_direction.y*p->move_direction.x - p->look_direction.x*p->move_direction.y);
    set_vel(p->phys, &world_move_dir, MOVE_SPEED);
}

static void rats_update(struct player_t *p, unsigned int dt, bool use_astar) {
    for(int i = 0; i < n_rats; i++) {
        bool line_of_sight = unobstructed(&rats[i]->phys->pos, &p->phys->pos, rats[i]->phys->radius);
        if(!line_of_sight && use_astar && diag_distance(p->phys->pos, rats[i]->phys->pos) < 20.0) {
            ipoint2 start = {(int)(rats[i]->phys->pos.x), (int)(rats[i]->phys->pos.y)};
            ipoint2 end = {(int)(p->phys->pos.x), (int)(p->phys->pos.y)};
            rats[i]->path = astar(start, end);
            node_t *start_node = pop_front(&rats[i]->path);
            free(start_node);
        }
        if(!line_of_sight && rats[i]->path) {
            point2 target = {(double)rats[i]->path->pt.x + 0.5, (double)rats[i]->path->pt.y + 0.5};
            bool reached = move_toward_point(rats[i]->phys, &target, RAT_SPEED, 0.1);
            if(reached) {
                node_t *reached_node = pop_front(&rats[i]->path);
                free(reached_node);
            }
        } else if(line_of_sight) {
            printf("Has line of sight\n");
            rats[i]->path = NULL;
            point2 target;
            target.x = p->phys->pos.x - (p->phys->pos.x - rats[i]->phys->pos.x) * 0.5;
            target.y = p->phys->pos.y - (p->phys->pos.y - rats[i]->phys->pos.y) * 0.5;
            move_toward_point(rats[i]->phys, &target, RAT_SPEED, 0.25);
        }
    }
}

static bool input(struct player_t *p)
{
        bool keep_running = true;

        vector2 move_dir = {0.0, 0.0};
        if(whitgl_input_pressed(WHITGL_INPUT_ESC))
            keep_running = false;
        if(whitgl_input_held(WHITGL_INPUT_UP))
            move_dir.x += 1.0;
        if(whitgl_input_held(WHITGL_INPUT_DOWN))
            move_dir.x -= 1.0;
        if(whitgl_input_held(WHITGL_INPUT_RIGHT))
            move_dir.y -= 1.0;
        if(whitgl_input_held(WHITGL_INPUT_LEFT))
            move_dir.y += 1.0;
        player->move_direction = move_dir;

        /*SDL_Event event;
	while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_MOUSEMOTION: {
                    p->angle += event.motion.xrel * MOUSE_SENSITIVITY / (double)(SCREEN_W * PIXEL_DIM);
                    break;
                }
                default:
                        break;
            }
	}*/
        return keep_running;
}

/*static GLfloat billboard_vertices[] = {
    // front
    -0.5, 0.0,  0.0,
    0.5, 0.0,  0.0,
    0.5,  1.0,  0.0,
    -0.5,  1.0,  0.0,
};

static GLfloat billboard_uv[] = {
    // front
    0.0f, 1.0f,
    1.0f, 1.0f,
    1.0f, 0.0f,
    0.0f, 0.0f,
};

static GLushort billboard_elements[] = {
    // front
    0, 1, 2,
    2, 3, 0
};*/

int main(int argc, char* argv[])
{
        WHITGL_LOG("Starting main.");
        whitgl_sys_setup setup = whitgl_sys_setup_zero;
        setup.cursor = CURSOR_DISABLE;
        setup.size.x = SCREEN_W;
        setup.size.y = SCREEN_H;
        setup.pixel_size = PIXEL_DIM;
        setup.name = "main";
        setup.resizable = false;
        if (!whitgl_sys_init(&setup))
            return 1;

        whitgl_shader tex_shader = whitgl_shader_zero;
        tex_shader.vertex_src = vertex_src;
        tex_shader.num_uniforms = 1;
        tex_shader.uniforms[0].name = "tex";
        tex_shader.uniforms[0].type = WHITGL_UNIFORM_IMAGE;
        if (!whitgl_change_shader(WHITGL_SHADER_TEXTURE, tex_shader))
            return 1;

	whitgl_shader post_shader = whitgl_shader_zero;
	post_shader.fragment_src = post_src;
	post_shader.num_uniforms = 2;
	post_shader.uniforms[0].name = "spread";
	post_shader.uniforms[0].type = WHITGL_UNIFORM_FLOAT;
	post_shader.uniforms[1].name = "tex";
	post_shader.uniforms[1].type = WHITGL_UNIFORM_IMAGE;
        /*if (!whitgl_change_shader(WHITGL_SHADER_POST, post_shader))
            return 1;*/


        /*whitgl_shader model_shader = whitgl_shader_zero;
	model_shader.fragment_src = model_src;
	if(!whitgl_change_shader(WHITGL_SHADER_MODEL, model_shader))
            return 1;*/


        whitgl_sys_enable_depth(true);
        
    //init();
        whitgl_sound_init();
        whitgl_input_init();

        whitgl_sound_add(0, "Assets/Sound/001rain.ogg");
        whitgl_sound_play(0, 1, 1);

        bool running = true;
        whitgl_sys_add_image(0, "Assets/Textures/wall.png");
	/*whitgl_random_seed seed = whitgl_random_seed_init(0);
        whitgl_ivec texture_size = {32,32};
        unsigned char data_texture[texture_size.x*texture_size.y*4];
        whitgl_int i;
        for(i = 0; i < texture_size.x*texture_size.y*4; i += 4) {
            whitgl_int pixel = i/4;
            data_texture[i] = (pixel%32)*8;
            data_texture[i+1] = (pixel/32)*8;
            data_texture[i+2] = whitgl_random_int(&seed, 32)+128;
            data_texture[i+3] = 255;
        }
	whitgl_sys_add_image_from_data(1, texture_size, data_texture);*/

        whitgl_load_model(0, "data/cube.wmd");

        whitgl_timer_init();
        whitgl_float time = 0;
        whitgl_float dt = 0;

        point2 p_pos = {1.5, 2};
        point2 rat_pos = {3, 3};
        player_create(&p_pos, 0.0);
        rat_create(&rat_pos);

	while (running) {
            whitgl_sound_update();
            whitgl_timer_tick();
            while (whitgl_timer_should_do_frame(60)) {
                dt = 1/60.0f;
                time += dt;
                whitgl_input_update();
                running = input(player);
                update(player, dt);

                //running = input(player);
                //update(player, dt);
                /*if(cycles_to_astar == 0) {
                    rats_update(player, dt, true);
                    cycles_to_astar = 100;
                } else {
                    rats_update(player, dt, false);
                    cycles_to_astar--;
                }*/
                phys_update(dt);
                //bind_fbo(fbo);
                //frame(player);
                //hud();
                //blit_fbo(fbo);
		//SDL_GL_SwapWindow(window);
            }
            frame(player, time);
            if(!whitgl_sys_window_focused())
                    whitgl_timer_sleep(1.0/30.0);


	}


        whitgl_input_shutdown();
        // Shutdown sound
        whitgl_sound_shutdown();
        whitgl_sys_close();

        /*ipoint2 start = {2, 2};
        ipoint2 end = {7, 7};
        node_t *n = astar(start, end);
        printf("Done\n");
        print_list(&n);*/
	return 0;
}
