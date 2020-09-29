#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>

#include <random>

static std::mt19937 mt; // usage based on 15-466-f20-base0 code

GLuint cube_pit_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > cube_pit_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("cube-pit.pnct"));
	cube_pit_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > cube_pit_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("cube-pit.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = cube_pit_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = cube_pit_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

Load< Sound::Sample > bgm_normal_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("random-bgm.wav"));
});

Load< Sound::Sample > bgm_low_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("random-bgm-low.wav"));
});

PlayMode::PlayMode() : scene(*cube_pit_scene) {
	// pointer to player object root
	for (auto &transform : scene.transforms)
	{
		if (transform.name == "Player")
		{
			std::cout << "Found Boxy!" << std::endl;
			boxy = &transform;
			std::cout << "Pos : " << boxy->position.z << std::endl;
		}
		else if (transform.name == "Cheese")
		{
			std::cout << "Found his food!" << std::endl;
			cheese = &transform;
			cheese->position.z = 0.2f;
		}
		
	}

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	//start music loop playing:
	bgm_normal_loop = Sound::loop_3D(*bgm_normal_sample, 1.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f);
	bgm_low_loop = Sound::loop_3D(*bgm_low_sample, 0.0f, glm::vec3(0.0f, 0.0f, 0.0f), 0.0f);
	Sound::listener.position = glm::vec3(0.0f, 0.0f, 0.0f);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_a) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_RETURN)
		{
			// Restart game
			timer = 3.0f;
			score = 0;
			//Restart the "good"
			bgm_normal_loop->volume = Sound::Ramp <float>(1.0f);
			bgm_low_loop->volume = Sound::Ramp <float>(0.0f);
			spawn_cheese();
			is_game_over = false;
			return true;
		}
	}
	else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	// Stop everything if game is over
	if (is_game_over)
	{
		return;
	}
	
	
	// move Boxy
	{
		//combine inputs into a move:	// Repurposing the camera move from base code to move player
		constexpr float PlayerSpeed = 30.0f;
		constexpr float RotationSpeed = 10.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = boxy->make_local_to_parent();
		glm::vec3 up = frame[1];

		boxy->position += move.y * up;

		boxy->rotation = boxy->rotation * glm::angleAxis(
			RotationSpeed * -move.x * elapsed,	// Rotation increases counter-clockwise. Hence *(-1)
			glm::vec3(0.0f, 0.0f, 1.0f)
		);
	}

	// timer update
	{
		if (timer <= 0)
		{
			timer = 0;
			bgm_normal_loop->volume = Sound::Ramp <float>(0.0f);
			bgm_low_loop->volume = Sound::Ramp <float>(1.0f);
			// Game over. Show the text
			is_game_over = true;
		}
		else
		{
			timer -= elapsed;
		}
	}

	// spin the cheese
	{
		constexpr float SpinSpeed = 5.0f;
		cheese->rotation = cheese->rotation * glm::angleAxis(SpinSpeed * elapsed, glm::vec3(0.0f, 0.0f, 1.0f));
	}

	// check for "collision". Essentially just checks for distance
	{
		float dist = glm::distance(cheese->position, boxy->position);
		if (dist <= 0.2f)
		{
			std::cout << "CHOMP!!!";

			score++;
			best_score = (score > best_score ? score : best_score);

			// Reset timer to a random number
			float randomNum = mt() / (float)mt.max();
			timer = 3.0f + randomNum * (2.0f);

			spawn_cheese();
		}
	}

	// TODO: Orbiting camera move
	{
		// cam_offset = camera->transform->position - boxy->position; // Do this in the constructor
		// camera->transform->position = boxy->position + cam_offset;
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	GL_ERRORS();
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	GL_ERRORS();
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	GL_ERRORS();
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("WASD to move Boxy. Eat cheese to survive. Time runs out, you die.",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("WASD to move Boxy. Eat cheese to survive. Time runs out, you die.",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));

		lines.draw_text("Timer:"+ std::to_string((int)std::ceil(timer)), // based on https://stackoverflow.com/a/35345427
			glm::vec3(-aspect + 0.1f * H, 0.8 - 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0xff));

		lines.draw_text("Score:"+ std::to_string(score),
			glm::vec3(0 + 0.1f * H, 0.8 - 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0xff));

		if (is_game_over)
		{
			constexpr float GameOverH = 0.2f;
			lines.draw_text("GAME OVER! Press Enter to restart.",
				glm::vec3(-aspect + 0.0f * GameOverH, -0.1f * GameOverH, 0.0),
				glm::vec3(GameOverH, 0.0f, 0.0f), glm::vec3(0.0f, GameOverH, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0xff));
			
			lines.draw_text("Best:"+ std::to_string(best_score),
				glm::vec3(-aspect + 0.0f * GameOverH, -2.0f * GameOverH, 0.0),
				glm::vec3(GameOverH, 0.0f, 0.0f), glm::vec3(0.0f, GameOverH, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0xff));
		}
		
	}
}

void PlayMode::spawn_cheese(){
	// based on https://glm.g-truc.net/0.9.4/api/a00154.html
			
	// Make the cheese disappear and reappear elsewhere
	glm::vec2 random_pt = glm::diskRand(FloorX);
	cheese->position.x = random_pt.x;
	cheese->position.y = random_pt.y;
}
