idf_component_register(
    SRCS "bench_main.c" "casos_pid.c" "casos_matrices.c" "casos_ekf.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_timer esp_rom
)
