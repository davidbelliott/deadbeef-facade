#include "game.h"
#include "intro.h"
#include "common.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <whitgl/sound.h>
#include <whitgl/input.h>
#include <whitgl/logging.h>
#include <whitgl/math.h>
#include <whitgl/sys.h>
#include <whitgl/random.h>
#include <whitgl/timer.h>

/*void makebillboard_mat4x4(mat4x4 BM, mat4x4 const MV);

static void mat4x4_print(mat4x4 m) {
    int i,j;
    for(i=0; i<4; ++i) {
        for(j=0; j<4; ++j)
            printf("%f, ", m[i][j]);
        printf("\n");
    }
}*/

const char* vertex_model_src = "\
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
uniform vec2 spritesheet_size;\
uniform vec2 sprite_offset;\
uniform vec2 sprite_size;\
void main()\
{\
	gl_Position = m_perspective * m_view * m_model * vec4( position, 1.0 );\
	Texturepos.x = (texturepos.x * sprite_size.x + sprite_offset.x) / spritesheet_size.x;\
	Texturepos.y = (texturepos.y * sprite_size.y + sprite_offset.y) / spritesheet_size.y;\
	fragmentColor = vertexColor;\
	fragmentNormal = vertexNormal;\
	fragmentPosition = vec3( m_model * vec4( position, 1.0));\
	normalMatrix = transpose(inverse(m_model));\
}\
";

const char* vertex_billboard_src = "\
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
uniform vec2 spritesheet_size;\
uniform vec2 sprite_offset;\
uniform vec2 sprite_size;\
void main()\
{\
        mat4 mv = m_view * m_model;\
        mv[0][0] = 1;\
        mv[0][1] = 0;\
        mv[0][2] = 0;\
        mv[1][0] = 0;\
        mv[1][1] = 1;\
        mv[1][2] = 0;\
        mv[2][0] = 0;\
        mv[2][1] = 0;\
        mv[2][2] = 1;\
	gl_Position = m_perspective * mv * vec4( position, 1.0 );\
	Texturepos.x = (texturepos.x * sprite_size.x + sprite_offset.x) / spritesheet_size.x;\
	Texturepos.y = (texturepos.y * sprite_size.y + sprite_offset.y) / spritesheet_size.y;\
	fragmentColor = vertexColor;\
	fragmentNormal = vertexNormal;\
	fragmentPosition = vec3( m_model * vec4( position, 1.0));\
	normalMatrix = transpose(inverse(m_model));\
}\
";

const char* frag_tex_src = "\
#version 150\
\n\
in vec2 Texturepos;\
in vec3 fragmentNormal;\
out vec4 outColor;\
uniform sampler2D tex;\
void main()\
{\
        vec4 tex_color = texture2D(tex, Texturepos);\
        if (tex_color.rgb == vec3(1.0, 0.0, 1.0))\
            discard;\
        outColor = tex_color;\
}\
";

const char* frag_src = "\
#version 150\
\n\
float fog_exp2(\
  const float dist,\
  const float density\
) {\
  const float LOG2 = -1.442695;\
  float d = density * dist;\
  return 1.0 - clamp(exp2(d * d * LOG2), 0.0, 1.0);\
}\
in vec2 Texturepos;\
in vec3 fragmentNormal;\
out vec4 outColor;\
uniform sampler2D tex;\
uniform float spread;\
uniform vec4 tint;\
void main()\
{\
        vec4 tex_color = texture2D(tex, Texturepos);\
        if (tex_color.rgb == vec3(1.0, 0.0, 1.0))\
            discard;\
        tex_color += tint;\
        float fogDistance = gl_FragCoord.z / gl_FragCoord.w;\
        float fogAmount = fog_exp2(fogDistance, 0.15);\
        float shadeFactor = dot(fragmentNormal.xy, vec2(0.2, 0.2));\
        vec4 color = (1.0 + shadeFactor) * tex_color * (1.0 - fogAmount);\
        outColor = vec4(color.r, color.g, color.b, 1.0);\
}\
";

const char* post_src = "\
#version 150\
\n\
float luma(vec3 color) {\
  return dot(color, vec3(0.299, 0.587, 0.114));\
}\
float luma(vec4 color) {\
  return dot(color.rgb, vec3(0.299, 0.587, 0.114));\
}\
float dither2x2(vec2 position, float brightness) {\
  int x = int(mod(position.x, 2.0));\
  int y = int(mod(position.y, 2.0));\
  int index = x + y * 2;\
  float limit = 0.0;\
  if (x < 8) {\
    if (index == 0) limit = 0.25;\
    if (index == 1) limit = 0.75;\
    if (index == 2) limit = 1.00;\
    if (index == 3) limit = 0.50;\
  }\
  return brightness < limit ? 0.0 : 1.0;\
}\
float dither4x4(vec2 position, float brightness) {\
  int x = int(mod(position.x, 4.0));\
  int y = int(mod(position.y, 4.0));\
  int index = x + y * 4;\
  float limit = 0.0;\
  if (x < 8) {\
    if (index == 0) limit = 0.0625;\
    if (index == 1) limit = 0.5625;\
    if (index == 2) limit = 0.1875;\
    if (index == 3) limit = 0.6875;\
    if (index == 4) limit = 0.8125;\
    if (index == 5) limit = 0.3125;\
    if (index == 6) limit = 0.9375;\
    if (index == 7) limit = 0.4375;\
    if (index == 8) limit = 0.25;\
    if (index == 9) limit = 0.75;\
    if (index == 10) limit = 0.125;\
    if (index == 11) limit = 0.625;\
    if (index == 12) limit = 1.0;\
    if (index == 13) limit = 0.5;\
    if (index == 14) limit = 0.875;\
    if (index == 15) limit = 0.375;\
  }\
  return brightness < limit ? 0.0 : 1.0;\
}\
in vec2 Texturepos;\
in vec3 fragmentNormal;\
out vec4 outColor;\
uniform sampler2D tex;\
uniform vec4 tint;\
void main()\
{\
        vec4 tex_color = texture2D(tex, Texturepos);\
        vec4 color = tex_color + tint;\
        ivec2 texsize = textureSize(tex, 0);\
        ivec2 itexpos = ivec2(texsize.x * Texturepos.x, texsize.y * Texturepos.y);\
        outColor = vec4(dither4x4(itexpos, color.r), dither4x4(itexpos, color.g), dither4x4(itexpos, color.b), 1.0);\
}\
";

void input(bool *paused, bool *running, int game_state) {
    /*if (whitgl_input_pressed(WHITGL_INPUT_ESC)) {
        *paused = !*paused;
        switch (game_state) {
            case GAME_STATE_GAME:
                game_pause(*paused);
                break;
            case GAME_STATE_INTRO:
                intro_pause(*paused);
                break;
            default:
                break;
        }
    }*/
    if (whitgl_input_pressed(WHITGL_INPUT_ESC)) {
        *running = false;
    }
}

int main(int argc, char* argv[])
{
        WHITGL_LOG("Starting main.");
        whitgl_sys_setup setup = whitgl_sys_setup_zero;
        setup.cursor = CURSOR_DISABLE;
        setup.size.x = SCREEN_W;
        setup.size.y = SCREEN_H;
        setup.pixel_size = PIXEL_DIM;
        setup.resolution_mode = RESOLUTION_EXACT;
        setup.name = "main";
        setup.resizable = false;
        if (!whitgl_sys_init(&setup))
            return 1;

        whitgl_shader tex_shader = whitgl_shader_zero;
        tex_shader.fragment_src = frag_tex_src;
        if (!whitgl_change_shader(WHITGL_SHADER_TEXTURE, tex_shader))
            return 1;

        whitgl_shader tex_model_shader = whitgl_shader_zero;
        tex_model_shader.vertex_src = vertex_model_src;
        tex_model_shader.fragment_src = frag_src;
        tex_model_shader.num_uniforms = 6;
        tex_model_shader.uniforms[0].name = "tex";
        tex_model_shader.uniforms[0].type = WHITGL_UNIFORM_IMAGE;
        tex_model_shader.uniforms[1].name = "spritesheet_size";
        tex_model_shader.uniforms[1].type = WHITGL_UNIFORM_FVEC;
        tex_model_shader.uniforms[2].name = "sprite_offset";
        tex_model_shader.uniforms[2].type = WHITGL_UNIFORM_FVEC;
        tex_model_shader.uniforms[3].name = "sprite_size";
        tex_model_shader.uniforms[3].type = WHITGL_UNIFORM_FVEC;
        tex_model_shader.uniforms[4].name = "tint";
        tex_model_shader.uniforms[4].type = WHITGL_UNIFORM_COLOR;
        if (!whitgl_change_shader(WHITGL_SHADER_EXTRA_0, tex_model_shader))
            return 1;

        whitgl_shader tex_billboard_shader = whitgl_shader_zero;
        tex_billboard_shader.vertex_src = vertex_billboard_src;
        tex_billboard_shader.fragment_src = frag_src;
        tex_billboard_shader.num_uniforms = 4;
        tex_billboard_shader.uniforms[0].name = "tex";
        tex_billboard_shader.uniforms[0].type = WHITGL_UNIFORM_IMAGE;
        tex_billboard_shader.uniforms[1].name = "spritesheet_size";
        tex_billboard_shader.uniforms[1].type = WHITGL_UNIFORM_FVEC;
        tex_billboard_shader.uniforms[2].name = "sprite_offset";
        tex_billboard_shader.uniforms[2].type = WHITGL_UNIFORM_FVEC;
        tex_billboard_shader.uniforms[3].name = "sprite_size";
        tex_billboard_shader.uniforms[3].type = WHITGL_UNIFORM_FVEC;
        if (!whitgl_change_shader(WHITGL_SHADER_EXTRA_1, tex_billboard_shader))
            return 1;

	whitgl_shader post_shader = whitgl_shader_zero;
	post_shader.fragment_src = post_src;
        post_shader.num_uniforms = 1;
        post_shader.uniforms[0].name = "tint";
        post_shader.uniforms[0].type = WHITGL_UNIFORM_COLOR;
        if (!whitgl_change_shader(WHITGL_SHADER_POST, post_shader))
            return 1;


        /*whitgl_shader model_shader = whitgl_shader_zero;
	model_shader.fragment_src = model_src;
	if(!whitgl_change_shader(WHITGL_SHADER_MODEL, model_shader))
            return 1;*/


        
        whitgl_sound_init();
        whitgl_input_init();

        whitgl_sys_color c = {0, 0, 0, 0};
        whitgl_set_shader_color(WHITGL_SHADER_POST, 0, c);


        bool running = true;
        whitgl_sys_add_image(0, "data/tex/tex.png");
        whitgl_set_shader_image(WHITGL_SHADER_EXTRA_0, 0, 0);
        whitgl_set_shader_image(WHITGL_SHADER_EXTRA_1, 0, 0);

        whitgl_ivec spritesheet_size = whitgl_sys_get_image_size(0);
        whitgl_fvec fspritesheet_size;
        fspritesheet_size.x = (whitgl_float)spritesheet_size.x;
        fspritesheet_size.y = (whitgl_float)spritesheet_size.y;
        whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_0, 1, fspritesheet_size);
        whitgl_set_shader_fvec(WHITGL_SHADER_EXTRA_1, 1, fspritesheet_size);

        whitgl_loop_add(0, "data/snd/metronome-short.ogg");
        whitgl_loop_add(AMBIENT_MUSIC, "data/snd/ambient.ogg");


	//whitgl_sound_add(0, "data/snd/tick.ogg");

        game_init();
        intro_init();


        int game_state = GAME_STATE_GAME;
        int next_state = game_state;
        whitgl_timer_init();
        whitgl_float time = 0.0f;
        whitgl_float dt = 0;
        bool paused = false;

        game_start();

	while (running) {
            whitgl_sound_update();
            dt = whitgl_timer_tick();
            whitgl_input_update();
            // Update
            if (!paused) {
                time += dt;
                switch (game_state) {
                    case GAME_STATE_GAME:
                        next_state = game_update(dt);
                        game_input();
                        break;
                    case GAME_STATE_INTRO:
                        next_state = intro_update(dt);
                        intro_input();
                        break;
                    default:
                        break;
                }
            }
            // Input
            input(&paused, &running, game_state);
            // Draw
            switch (game_state) {
                case GAME_STATE_GAME:
                    game_frame();
                    break;
                case GAME_STATE_INTRO:
                    intro_frame();
                    break;
                default:
                    break;
            }
            // State change logic
            if (next_state != game_state) {
                // Exit logic
                switch (game_state) {
                    case GAME_STATE_GAME:
                        game_stop();
                        break;
                    case GAME_STATE_INTRO:
                        break;
                    default:
                        break;
                }
                // Entrance logic
                switch (next_state) {
                    case GAME_STATE_GAME:
                        game_start();
                        break;
                    case GAME_STATE_INTRO:
                        intro_start();
                        break;
                    default:
                        break;
                }
                game_state = next_state;
            }
        }

        game_cleanup();
        intro_cleanup();

        whitgl_input_shutdown();
        // Shutdown sound
        whitgl_sound_shutdown();
        whitgl_sys_close();

	return 0;
}
