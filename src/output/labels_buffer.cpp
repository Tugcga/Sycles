#include <xsi_application.h>

#include "labels_symbols.h"
#include "../render_base/image_buffer.h"

void add_symbol(ImageBuffer* buffer, size_t bottom_row, const float* array, size_t height, size_t width, size_t shift, const float* color)
{
	size_t channels = buffer->get_channels();
	float* buffer_pixels = buffer->get_pixels_pointer();
	size_t label_point_iterator = 0;
	for (size_t y = 4 + bottom_row; y < height + 4 + bottom_row; y++)
	{
		for (size_t x = 4 + shift; x < 4 + shift + width; x++)
		{
			size_t p = buffer->get_pixel_index(x, y);
			size_t label_p = (height - y + 4 + bottom_row - 1) * width + x - 4 - shift;
			for (size_t c = 0; c < channels; c++)
			{
				float v = buffer_pixels[p * channels + c];  // curent value of the pixel channel
				buffer_pixels[p * channels + c] = (color[c] * array[label_p]) * (1 - v) + v;
			}
			label_point_iterator++;
		}
	}
}

void build_labels_buffer(ImageBuffer* buffer, 
	const XSI::CString &text_string, 
	size_t image_width, size_t image_height, 
	size_t horisontal_shift,
	size_t box_height, 
	float back_r, float back_g, float back_b, float back_a, 
	size_t bottom_row)
{
	float text_color[4] = { 0.8, 0.8, 0.8, 1 };
	float* buffer_pixels = buffer->get_pixels_pointer();
	for (size_t y = bottom_row; y < box_height + bottom_row; y++)
	{
		for (size_t x = 0; x < image_width; x++)
		{
			size_t p = buffer->get_pixel_index(x, y);
			buffer_pixels[p * 4] = back_r;
			buffer_pixels[p * 4 + 1] = back_g;
			buffer_pixels[p * 4 + 2] = back_b;
			buffer_pixels[p * 4 + 3] = back_a;
		}
	}

	size_t line_length = text_string.Length();
	for (size_t i = 0; i < line_length; i++)
	{
		if (text_string[i] == 'A') { add_symbol(buffer, bottom_row, symbol_a_c, symbol_height, symbol_a_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_a_c_width; }
		else if (text_string[i] == 'B') { add_symbol(buffer, bottom_row, symbol_b_c, symbol_height, symbol_b_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_b_c_width; }
		else if (text_string[i] == 'C') { add_symbol(buffer, bottom_row, symbol_c_c, symbol_height, symbol_c_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_c_c_width; }
		else if (text_string[i] == 'D') { add_symbol(buffer, bottom_row, symbol_d_c, symbol_height, symbol_d_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_d_c_width; }
		else if (text_string[i] == 'E') { add_symbol(buffer, bottom_row, symbol_e_c, symbol_height, symbol_e_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_e_c_width; }
		else if (text_string[i] == 'F') { add_symbol(buffer, bottom_row, symbol_f_c, symbol_height, symbol_f_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_f_c_width; }
		else if (text_string[i] == 'G') { add_symbol(buffer, bottom_row, symbol_g_c, symbol_height, symbol_g_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_g_c_width; }
		else if (text_string[i] == 'H') { add_symbol(buffer, bottom_row, symbol_h_c, symbol_height, symbol_h_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_h_c_width; }
		else if (text_string[i] == 'I') { add_symbol(buffer, bottom_row, symbol_i_c, symbol_height, symbol_i_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_i_c_width; }
		else if (text_string[i] == 'J') { add_symbol(buffer, bottom_row, symbol_j_c, symbol_height, symbol_j_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_j_c_width; }
		else if (text_string[i] == 'K') { add_symbol(buffer, bottom_row, symbol_k_c, symbol_height, symbol_k_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_k_c_width; }
		else if (text_string[i] == 'L') { add_symbol(buffer, bottom_row, symbol_l_c, symbol_height, symbol_l_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_l_c_width; }
		else if (text_string[i] == 'M') { add_symbol(buffer, bottom_row, symbol_m_c, symbol_height, symbol_m_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_m_c_width; }
		else if (text_string[i] == 'N') { add_symbol(buffer, bottom_row, symbol_n_c, symbol_height, symbol_n_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_n_c_width; }
		else if (text_string[i] == 'O') { add_symbol(buffer, bottom_row, symbol_o_c, symbol_height, symbol_o_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_o_c_width; }
		else if (text_string[i] == 'P') { add_symbol(buffer, bottom_row, symbol_p_c, symbol_height, symbol_p_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_p_c_width; }
		else if (text_string[i] == 'Q') { add_symbol(buffer, bottom_row, symbol_q_c, symbol_height, symbol_q_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_q_c_width; }
		else if (text_string[i] == 'R') { add_symbol(buffer, bottom_row, symbol_r_c, symbol_height, symbol_r_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_r_c_width; }
		else if (text_string[i] == 'S') { add_symbol(buffer, bottom_row, symbol_s_c, symbol_height, symbol_s_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_s_c_width; }
		else if (text_string[i] == 'T') { add_symbol(buffer, bottom_row, symbol_t_c, symbol_height, symbol_t_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_t_c_width; }
		else if (text_string[i] == 'U') { add_symbol(buffer, bottom_row, symbol_u_c, symbol_height, symbol_u_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_u_c_width; }
		else if (text_string[i] == 'V') { add_symbol(buffer, bottom_row, symbol_v_c, symbol_height, symbol_v_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_v_c_width; }
		else if (text_string[i] == 'W') { add_symbol(buffer, bottom_row, symbol_w_c, symbol_height, symbol_w_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_w_c_width; }
		else if (text_string[i] == 'X') { add_symbol(buffer, bottom_row, symbol_x_c, symbol_height, symbol_x_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_x_c_width; }
		else if (text_string[i] == 'Y') { add_symbol(buffer, bottom_row, symbol_y_c, symbol_height, symbol_y_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_y_c_width; }
		else if (text_string[i] == 'Z') { add_symbol(buffer, bottom_row, symbol_z_c, symbol_height, symbol_z_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_z_c_width; }
		else if (text_string[i] == '0') { add_symbol(buffer, bottom_row, symbol_0, symbol_height, symbol_0_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_0_width; }
		else if (text_string[i] == '1') { add_symbol(buffer, bottom_row, symbol_1, symbol_height, symbol_1_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_1_width; }
		else if (text_string[i] == '2') { add_symbol(buffer, bottom_row, symbol_2, symbol_height, symbol_2_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_2_width; }
		else if (text_string[i] == '3') { add_symbol(buffer, bottom_row, symbol_3, symbol_height, symbol_3_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_3_width; }
		else if (text_string[i] == '4') { add_symbol(buffer, bottom_row, symbol_4, symbol_height, symbol_4_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_4_width; }
		else if (text_string[i] == '5') { add_symbol(buffer, bottom_row, symbol_5, symbol_height, symbol_5_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_5_width; }
		else if (text_string[i] == '6') { add_symbol(buffer, bottom_row, symbol_6, symbol_height, symbol_6_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_6_width; }
		else if (text_string[i] == '7') { add_symbol(buffer, bottom_row, symbol_7, symbol_height, symbol_7_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_7_width; }
		else if (text_string[i] == '8') { add_symbol(buffer, bottom_row, symbol_8, symbol_height, symbol_8_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_8_width; }
		else if (text_string[i] == '9') { add_symbol(buffer, bottom_row, symbol_9, symbol_height, symbol_9_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_9_width; }
		else if (text_string[i] == ' ') { horisontal_shift = horisontal_shift + 8; }
		else if (text_string[i] == '|') { add_symbol(buffer, bottom_row, symbol_bar, symbol_height_low_case, symbol_bar_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_bar_width; }
		else if (text_string[i] == ':') { add_symbol(buffer, bottom_row, symbol_dotDot, symbol_height, symbol_dotDot_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_dotDot_width; }
		else if (text_string[i] == ';') { add_symbol(buffer, bottom_row, symbol_prDot, symbol_height, symbol_prDot_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_prDot_width; }
		else if (text_string[i] == ',') { add_symbol(buffer, bottom_row, symbol_pr, symbol_height, symbol_pr_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_pr_width; }
		else if (text_string[i] == '.') { add_symbol(buffer, bottom_row, symbol_dot, symbol_height, symbol_dot_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_dot_width; }
		else if (text_string[i] == 'a') { add_symbol(buffer, bottom_row, symbol_a, symbol_height_low_case, symbol_a_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_a_width; }
		else if (text_string[i] == 'b') { add_symbol(buffer, bottom_row, symbol_b, symbol_height_low_case, symbol_b_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_b_width; }
		else if (text_string[i] == 'c') { add_symbol(buffer, bottom_row, symbol_c, symbol_height_low_case, symbol_c_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_c_width; }
		else if (text_string[i] == 'd') { add_symbol(buffer, bottom_row, symbol_d, symbol_height_low_case, symbol_d_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_d_width; }
		else if (text_string[i] == 'e') { add_symbol(buffer, bottom_row, symbol_e, symbol_height_low_case, symbol_e_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_e_width; }
		else if (text_string[i] == 'f') { add_symbol(buffer, bottom_row, symbol_f, symbol_height_low_case, symbol_f_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_f_width; }
		else if (text_string[i] == 'g') { add_symbol(buffer, bottom_row, symbol_g, symbol_height_low_case, symbol_g_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_g_width; }
		else if (text_string[i] == 'h') { add_symbol(buffer, bottom_row, symbol_h, symbol_height_low_case, symbol_h_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_h_width; }
		else if (text_string[i] == 'i') { add_symbol(buffer, bottom_row, symbol_i, symbol_height_low_case, symbol_i_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_i_width; }
		else if (text_string[i] == 'j') { add_symbol(buffer, bottom_row, symbol_j, symbol_height_low_case, symbol_j_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_j_width; }
		else if (text_string[i] == 'k') { add_symbol(buffer, bottom_row, symbol_k, symbol_height_low_case, symbol_k_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_k_width; }
		else if (text_string[i] == 'l') { add_symbol(buffer, bottom_row, symbol_l, symbol_height_low_case, symbol_l_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_l_width; }
		else if (text_string[i] == 'm') { add_symbol(buffer, bottom_row, symbol_m, symbol_height_low_case, symbol_m_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_m_width; }
		else if (text_string[i] == 'n') { add_symbol(buffer, bottom_row, symbol_n, symbol_height_low_case, symbol_n_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_n_width; }
		else if (text_string[i] == 'o') { add_symbol(buffer, bottom_row, symbol_o, symbol_height_low_case, symbol_o_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_o_width; }
		else if (text_string[i] == 'p') { add_symbol(buffer, bottom_row, symbol_p, symbol_height_low_case, symbol_p_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_p_width; }
		else if (text_string[i] == 'q') { add_symbol(buffer, bottom_row, symbol_q, symbol_height_low_case, symbol_q_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_q_width; }
		else if (text_string[i] == 'r') { add_symbol(buffer, bottom_row, symbol_r, symbol_height_low_case, symbol_r_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_r_width; }
		else if (text_string[i] == 's') { add_symbol(buffer, bottom_row, symbol_s, symbol_height_low_case, symbol_s_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_s_width; }
		else if (text_string[i] == 't') { add_symbol(buffer, bottom_row, symbol_t, symbol_height_low_case, symbol_t_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_t_width; }
		else if (text_string[i] == 'u') { add_symbol(buffer, bottom_row, symbol_u, symbol_height_low_case, symbol_u_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_u_width; }
		else if (text_string[i] == 'v') { add_symbol(buffer, bottom_row, symbol_v, symbol_height_low_case, symbol_v_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_v_width; }
		else if (text_string[i] == 'w') { add_symbol(buffer, bottom_row, symbol_w, symbol_height_low_case, symbol_w_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_w_width; }
		else if (text_string[i] == 'x') { add_symbol(buffer, bottom_row, symbol_x, symbol_height_low_case, symbol_x_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_x_width; }
		else if (text_string[i] == 'y') { add_symbol(buffer, bottom_row, symbol_y, symbol_height_low_case, symbol_y_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_y_width; }
		else if (text_string[i] == 'z') { add_symbol(buffer, bottom_row, symbol_z, symbol_height_low_case, symbol_z_width, horisontal_shift, text_color); horisontal_shift = horisontal_shift + symbol_z_width; }
	}
}