#include <SFML\Graphics.hpp>
#include <iostream>
#include <cmath>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <climits>

/// point
struct point {
	float x;
	float y;

	point() {}

	point(float x, float y) : x(x), y(y) {}
};
///

/// some consts
const float PI = 3.14159265;
const float SMALL_ANGLE = 0.0001;
const float SMALL_DISTANCE = 0.01;
///

/// resolution info
const unsigned RESOLUTION_WIDTH = 640;
const unsigned RESOLUTION_HIGHT = 480;
const unsigned RAY_SLICE = 8;
///

/// player info
// if we put whole number for an angle, we would at one point calculate tan(90), which is inf...
point player_point(127.0, 128.0);
double player_angle = SMALL_ANGLE;
double player_hight_view = 0.0;
const float SPEED = 1.0;
const float SENSITIVITY_H = 10.0;
const float SENSITIVITY_V = 50.0;
const float PLAYER_VIEW_FIELD_ANGLE = 60.0;
// 480 (player_view_field_angle * ray_slice) / 640 (resolution_width) = 0.75
const float ANGLE_SLICE = PLAYER_VIEW_FIELD_ANGLE * RAY_SLICE / RESOLUTION_WIDTH;
///

/// fast square root
float fast_sqrt(const float &number) {
	// variable declaration
	long float_bit_representation;
	float one_over_sqrt;

	// approximate calculation of (1 / sqrt(number)) via bit shifting
	one_over_sqrt = number;
	float_bit_representation = *(long*)&one_over_sqrt;
	float_bit_representation = 0x5f3759df - (float_bit_representation >> 1);
	one_over_sqrt = *(float*)&float_bit_representation;

	// returns sqrt(number) with one iteration of Newton's interpolation
	return 1.0 / (one_over_sqrt *
		(1.5 - number * 0.5 * one_over_sqrt * one_over_sqrt));
}
///

/// check if one point is between tw other points
// optimize
bool is_between(const point &currPoint, const point &point1, const point &point2) {
	// calculates difference
	float x_difference = point2.x - point1.x;
	float y_difference = point2.y - point1.y;

	// main condition
	if (abs(x_difference) >= abs(y_difference))
		return x_difference > 0 ?
		point1.x <= currPoint.x && currPoint.x <= point2.x :
		point2.x <= currPoint.x && currPoint.x <= point1.x;
	else
		return y_difference > 0 ?
		point1.y <= currPoint.y && currPoint.y <= point2.y :
		point2.y <= currPoint.y && currPoint.y <= point1.y;
}
///

/// calculates distance between two points
float distance(const point &point1, const point &point2) {
	// return value of distance between two point
	return fast_sqrt((point1.x - point2.x) * (point1.x - point2.x) +
		(point1.y - point2.y) * (point1.y - point2.y));
}
///

/// random with bounds
int bounded_rand(const int lower, const int upper) {
	// returns value of random number between given points
	return lower + (upper - lower) * (rand() / (double)RAND_MAX);
}
///

/// precalculation of wall directions and offests
void precalculate_wall_info(std::ifstream &is_wall_edges) {
	// variable declaration
	point first_wall_edge;
	point second_wall_edge;
	float wall_direction;
	float wall_y_offset;

	// opening file stream with wall directions and offsets
	std::ofstream os_wall_directions_offsets;
	os_wall_directions_offsets.open("input_wall_d_o.txt");

	// one wall per iteration
	while (!is_wall_edges.eof()) {
		// reading wall edges
		is_wall_edges >> first_wall_edge.x >> first_wall_edge.y >>
			second_wall_edge.x >> second_wall_edge.y;

		// calculating wall direction and offset
		wall_direction = (second_wall_edge.y - first_wall_edge.y) /
			(second_wall_edge.x - first_wall_edge.x == 0 ?
				SMALL_DISTANCE :
				second_wall_edge.x - first_wall_edge.x);
		wall_y_offset = second_wall_edge.y - wall_direction * second_wall_edge.x;

		// writing wall direction and offset into file stream
		os_wall_directions_offsets << wall_direction << ' ' << wall_y_offset << std::endl;
	}

	// closing file stream
	os_wall_directions_offsets.close();

	// opening file stream with precalculation flag
	std::ofstream os_flag;
	os_flag.open("input_flag.txt");

	// writing 1 as a new flag into file
	os_flag << 1;

	// closing file stream
	os_flag.close();
}
///

/// input handler
// handle diagonal movement manualy
void move_player() {
	// player movement
	float x = 0.0;
	float y = 0.0;
	double angle = -player_angle * PI / 180.0;
	float scaled_speed = 0.1 * SPEED;
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) {
		x = scaled_speed * cos(angle);
		y = -scaled_speed * sin(angle);
	}
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) {
		x = -scaled_speed * cos(angle);
		y = scaled_speed * sin(angle);
	}
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) {
		x = -scaled_speed * sin(angle);
		y = -scaled_speed * cos(angle);
	}
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) {
		x = scaled_speed * sin(angle);
		y = scaled_speed * cos(angle);
	}
	player_point.x += x;
	player_point.y += y;

	// camera rotation
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::J))
		player_angle = fmod(360.0 + player_angle - 0.1 * SENSITIVITY_H, 360.0);
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::L))
		player_angle = fmod(player_angle + 0.1 * SENSITIVITY_H, 360.0);
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::I) && player_hight_view < RESOLUTION_HIGHT / 2)
		player_hight_view += 0.1 * SENSITIVITY_V;
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::K) && -player_hight_view < RESOLUTION_HIGHT / 2)
		player_hight_view -= 0.1 * SENSITIVITY_V;
}
///

/// per frame calculations
// calculate ray by ray, and if u get the same k and n, dont draw anything, keep on doing that, and at the end draw only polygon
std::vector<sf::RectangleShape> frame_calculate(std::ifstream &is_wall_edges, std::ifstream &is_wall_directions_offsets) {
	// vector of all slices which make one frame
	std::vector<sf::RectangleShape> frame;

	// starting angle for ray-casting
	float left_camera_angle = player_angle - PLAYER_VIEW_FIELD_ANGLE / 2;

	// one ray per iteration
	for (unsigned it = 0; it < RESOLUTION_WIDTH; it += RAY_SLICE) {
		float min_distance = FLT_MAX;

		// wall info
		point first_wall_edge;
		point second_wall_edge;
		float wall_direction;
		float wall_offset;

		// one wall per iteration
		while (!is_wall_edges.eof()) {
			// reading wall info
			is_wall_edges >> first_wall_edge.x >> first_wall_edge.y >>
				second_wall_edge.x >> second_wall_edge.y;
			is_wall_directions_offsets >> wall_direction >> wall_offset;

			// ray direction calculation
			float ray_direction = tan((left_camera_angle + ANGLE_SLICE * (it / RAY_SLICE)) * PI / 180.0);

			// finds intersect point
			point intersect_point;
			intersect_point.x = (player_point.y - wall_offset - ray_direction * player_point.x) /
				(wall_direction - ray_direction);
			intersect_point.y = wall_direction * intersect_point.x + wall_offset;

			// checks if intersect is behind the player. optimize maybe?
			float player_normal_direction = tan((player_angle + 90.0) * PI / 180.0);
			float player_normal_offset = player_point.y - player_normal_direction * player_point.x;
			if (player_angle > 0.0 && player_angle < 180.0 ?
				intersect_point.y - player_normal_direction * intersect_point.x - player_normal_offset < 0.0 :
				intersect_point.y - player_normal_direction * intersect_point.x - player_normal_offset > 0.0)
				continue;

			// checks if intersect point is actually between two edges of the wall
			if (!is_between(intersect_point, first_wall_edge, second_wall_edge))
				continue;

			// calculates the distance between player and intersect point, we can limit view distance by limiting z
			float distance_player_intersect = distance(player_point, intersect_point);
			if (min_distance > distance_player_intersect)
				min_distance = distance_player_intersect;
		}

		// file streams reset
		is_wall_edges.clear();
		is_wall_edges.seekg(0, is_wall_edges.beg);
		is_wall_directions_offsets.clear();
		is_wall_directions_offsets.seekg(0, is_wall_edges.beg);

		// we havent find any intersect point along current ray
		if (min_distance == FLT_MAX)
			continue;

		// calculates the length of the stripe found by ray
		float stripe_length = RESOLUTION_HIGHT / (min_distance + SMALL_DISTANCE);

		// preparing stripe for rendering
		sf::RectangleShape stripe = sf::RectangleShape(sf::Vector2f(RAY_SLICE, stripe_length));
		stripe.setOrigin(0, stripe_length / 2);
		stripe.setPosition(it, RESOLUTION_HIGHT / 2 + player_hight_view);
		//stripe.setFillColor(sf::Color(bounded_rand(0, 255), bounded_rand(0, 255), bounded_rand(0, 255)));
		stripe.setFillColor(sf::Color(128, 96, 64));
		frame.push_back(stripe);
	}

	// returns vector consisting of all stripes needed for one frame
	return frame;
}
///

/// per frame output
void draw_frame(sf::RenderWindow &window, const std::vector<sf::RectangleShape> &frame) {
	// clear previous frame
	window.clear();

	// ceiling
	sf::RectangleShape ceiling(sf::Vector2f(RESOLUTION_WIDTH, RESOLUTION_HIGHT));
	ceiling.setFillColor(sf::Color(64, 64, 64));
	ceiling.setOrigin(0, RESOLUTION_HIGHT);
	ceiling.setPosition(0, RESOLUTION_HIGHT / 2 + player_hight_view);
	window.draw(ceiling);

	// floor
	sf::RectangleShape floor(sf::Vector2f(RESOLUTION_WIDTH, RESOLUTION_HIGHT));
	floor.setFillColor(sf::Color(128, 128, 128));
	floor.setPosition(0, RESOLUTION_HIGHT / 2 + player_hight_view);
	window.draw(floor);

	// walls
	for (unsigned it = 0; it < frame.size(); window.draw(frame[it++]));

	// track of the player
	std::cout << '(' << player_point.x << ", " << player_point.y << ")  " << player_angle << std::endl;

	// display nexy frame
	window.display();
}
///

/// main
int main(void) {
	// opening file stream with precalculation flag
	std::ifstream is_flag;
	is_flag.open("input_flag.txt");

	// reading the flag
	int precalc_flag;
	is_flag >> precalc_flag;

	// closing file stream
	is_flag.close();

	// opening file stream with coordinates of wall edges
	std::ifstream is_wall_edges;
	is_wall_edges.open("input_wall_e.txt");

	// checking if precalculation is needed
	if (precalc_flag == 0)
		precalculate_wall_info(is_wall_edges);

	// opening file stream with wall directions and offsets
	std::ifstream is_wall_directions_offsets;
	is_wall_directions_offsets.open("input_wall_d_o.txt");

	// window creation
	sf::RenderWindow window(sf::VideoMode(RESOLUTION_WIDTH, RESOLUTION_HIGHT), "qrchina");

	// update loop
	while (window.isOpen()) {
		// player movement
		move_player();

		// frame drawing
		draw_frame(window, frame_calculate(is_wall_edges, is_wall_directions_offsets));
	}

	// closing file streams
	is_wall_edges.close();
	is_wall_directions_offsets.close();
}
///