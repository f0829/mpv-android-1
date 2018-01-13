#pragma once

#include <nettle/mini-gmp.h>
#define mpz_div_2exp(q,d,e) mpz_tdiv_q_2exp(q,d,e)
#define mpz_mod_2exp(q,d,e) mpz_tdiv_r_2exp(q,d,e)
