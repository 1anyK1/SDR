#include "./common.h"
#include <algorithm>

SDRData g_sdr_data;

void run_gui() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    SDL_Window* window = SDL_CreateWindow(
        "SDR Real-time Monitor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1400, 800, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    std::vector<float> display_i, display_q, display_time;
    std::vector<float> display_mag, display_phase;
    std::vector<float> constellation_i, constellation_q;

    size_t max_display_points = 2000;
    bool auto_scale = true;
    float scale_min = -10000.0f, scale_max = 10000.0f;

    struct Stats {
        size_t total_samples = 0;
        float max_i = 0, max_q = 0;
        float avg_i = 0, avg_q = 0;
        float rms = 0;
    } stats;
    
    bool running = true;
    
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        if (g_sdr_data.has_new_data()) {
            auto samples = g_sdr_data.get_samples_copy();
            size_t num_iq_pairs = samples.size() / 2;
            
            if (num_iq_pairs > 0) {
                display_i.clear();
                display_q.clear();
                display_time.clear();
                display_mag.clear();
                display_phase.clear();
                constellation_i.clear();
                constellation_q.clear();

                size_t step = 1;
                if (num_iq_pairs > max_display_points) {
                    step = num_iq_pairs / max_display_points;
                }

                stats.total_samples = num_iq_pairs;
                stats.max_i = 0; stats.max_q = 0;
                stats.avg_i = 0; stats.avg_q = 0;
                stats.rms = 0;
                
                size_t display_count = 0;
                
                for (size_t i = 0; i < num_iq_pairs; i += step) {
                    float i_val = static_cast<float>(samples[i * 2]);
                    float q_val = static_cast<float>(samples[i * 2 + 1]);
                    
                    display_i.push_back(i_val);
                    display_q.push_back(q_val);
                    display_time.push_back(static_cast<float>(i));
                    
                    float mag = std::sqrt(i_val*i_val + q_val*q_val);
                    float phase = std::atan2(q_val, i_val);
                    display_mag.push_back(mag);
                    display_phase.push_back(phase);

                    if (i % 5 == 0 && constellation_i.size() < 1000) {
                        constellation_i.push_back(i_val);
                        constellation_q.push_back(q_val);
                    }
                    
                    stats.max_i = std::max(stats.max_i, std::abs(i_val));
                    stats.max_q = std::max(stats.max_q, std::abs(q_val));
                    stats.avg_i += i_val;
                    stats.avg_q += q_val;
                    stats.rms += (i_val*i_val + q_val*q_val);
                    display_count++;
                }

                if (display_count > 0) {
                    stats.avg_i /= display_count;
                    stats.avg_q /= display_count;
                    stats.rms = std::sqrt(stats.rms / display_count);

                    if (auto_scale && !display_i.empty()) {
                        auto [min_i, max_i] = std::minmax_element(display_i.begin(), display_i.end());
                        auto [min_q, max_q] = std::minmax_element(display_q.begin(), display_q.end());
                        float abs_max = std::max(std::abs(*min_i), std::abs(*max_i));
                        abs_max = std::max(abs_max, std::max(std::abs(*min_q), std::abs(*max_q)));
                        
                        scale_min = -abs_max * 1.1f;
                        scale_max = abs_max * 1.1f;
                    }
                }
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_None);

        {
            ImGui::Begin("I/Q Signal Plots");
            
            if (ImPlot::BeginPlot("I and Q Channels", ImVec2(-1, 300))) {
                ImPlot::SetupAxes("Sample Index", "Amplitude");
                ImPlot::SetupAxisLimits(ImAxis_Y1, scale_min, scale_max, ImGuiCond_Always);
                
                if (!display_i.empty()) {
                    ImPlot::SetNextLineStyle(ImVec4(1.0f, 0.5f, 0.0f, 1.0f)); // Оранжевый для I
                    ImPlot::PlotLine("I Channel", display_time.data(), display_i.data(), display_i.size());
                    
                    ImPlot::SetNextLineStyle(ImVec4(0.0f, 0.8f, 1.0f, 1.0f)); // Голубой для Q
                    ImPlot::PlotLine("Q Channel", display_time.data(), display_q.data(), display_q.size());
                }
                
                ImPlot::EndPlot();
            }
            ImGui::End();
        }

        {
            ImGui::Begin("Constellation Diagram");
            
            if (ImPlot::BeginPlot("IQ Constellation", ImVec2(-1, 400))) {
                ImPlot::SetupAxes("I (In-phase)", "Q (Quadrature)");
                ImPlot::SetupAxisLimits(ImAxis_X1, scale_min, scale_max);
                ImPlot::SetupAxisLimits(ImAxis_Y1, scale_min, scale_max);
                
                if (!constellation_i.empty()) {
                    ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 3, 
                        ImVec4(0.0f, 1.0f, 0.0f, 0.5f), IMPLOT_AUTO, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                    ImPlot::PlotScatter("IQ Points", constellation_i.data(), constellation_q.data(), 
                                       constellation_i.size());
                }
                
                ImPlot::EndPlot();
            }
            
            ImGui::End();
        }

        ImGui::Render();
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}