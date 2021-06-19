/********************************************************************************************
* SIDH: an efficient supersingular isogeny cryptography library
*
* Abstract: Pohlig-Hellman with optimal strategy
*********************************************************************************************/



void from_base(int *D, digit_t *r, int Dlen, int base) 
{ // Convert a number in base "base" with signed digits: (D[k-1]D[k-2]...D[1]D[0])_base < 2^(NWORDS_ORDER*RADIX) into decimal 
  // Output: r = (D[k-1]*base^(k-1) + ... + D[1]*base + D[0])_10
    digit_t ell[NWORDS_ORDER] = {0}, digit[NWORDS_ORDER] = {0}, temp[NWORDS_ORDER] = {0};    
    int ellw;
    
    ell[0] = base;
    if (D[Dlen-1] < 0) {
        digit[0] = (digit_t)(((int)-D[Dlen-1])*ell[0]);
        if ((base & 1) == 0) {
            Montgomery_neg(digit, (digit_t*)Alice_order);
            copy_words(digit, r, NWORDS_ORDER);
        } else {
            mp_sub((digit_t*)Bob_order, digit, r, NWORDS_ORDER);             
        }        
    } else {
        r[0] = (digit_t)(D[Dlen-1]*ell[0]);
    }

    for (int i = Dlen-2; i >= 1; i--) {
        ellw = base;
        memset(digit,0,NWORDS_ORDER*sizeof(digit_t));
        if (D[i] < 0) {
            digit[0] = (digit_t)(-D[i]);
            if ((base & 1) == 0) {
                Montgomery_neg(digit, (digit_t*)Alice_order);
            } else {  
                mp_sub((digit_t*)Bob_order, digit, digit, NWORDS_ORDER);                            
            }
        } else {
            digit[0] = (digit_t)D[i];
        }
        mp_add(r, digit, r, NWORDS_ORDER);
        if ((base & 1) != 0) {
            if (!is_orderelm_lt(r, (digit_t*)Bob_order))
                mp_sub(r,(digit_t*)Bob_order, r, NWORDS_ORDER);               
        }
        
        if ((base & 1) == 0) {
            while (ellw > 1) {
                mp_add(r, r, r, NWORDS_ORDER);   
                ellw /= 2;
            }
        } else {
            while (ellw > 1) {
                memset(temp,0,NWORDS_ORDER*sizeof(digit_t));
                mp_add(r, r, temp, NWORDS_ORDER);
                if (!is_orderelm_lt(temp, (digit_t*)Bob_order)) 
                    mp_sub(temp,(digit_t*)Bob_order, temp, NWORDS_ORDER);
                
                mp_add(r, temp, r, NWORDS_ORDER);

                if (!is_orderelm_lt(r, (digit_t*)Bob_order))
                    mp_sub(r,(digit_t*)Bob_order, r, NWORDS_ORDER);                
                ellw /= 3;
            }
        }
    }
    memset(digit,0,NWORDS_ORDER*sizeof(digit_t));
    if (D[0] < 0) {
        digit[0] = (digit_t)(-D[0]);
        if ((base & 1) == 0) {
            Montgomery_neg(digit, (digit_t*)Alice_order);
        } else { 
            mp_sub((digit_t*)Bob_order, digit, digit, NWORDS_ORDER);            
        }
    } else {
        digit[0] = (digit_t)D[0];
    }
    mp_add(r, digit, r, NWORDS_ORDER);
    if ((base & 1) != 0) { 
        if (!is_orderelm_lt(r, (digit_t*)Bob_order))
            mp_sub(r,(digit_t*)Bob_order, r, NWORDS_ORDER);     
    }
}



#ifdef COMPRESSED_TABLES

#ifdef ELL2_TORUS

int ord2w_dlog(const felm_t *r, const int *logT, const felm_t *Texp)
{ // Given a generator rho_2 of order 2^w1, input elements r=[x:y] and w1, compute d <- log_{rho_2}r, 
  // where r = [x,y] \equiv [a_k:1] for some k in {1,2,..2^w1-1} or r \equiv [1:0]
  // Output: corresponding digit d in [-2^{w1-1},2^{w1-1}]
    felm_t x, y;
    felm_t sum = {0}, prods[1<<(W_2_1-1)] = {0};

    fpcopy(r[0], x);
    fpcopy(r[1], y);
    fpcorrection(x);
    fpcorrection(y);

    if (is_felm_zero(y)) return 0;
    if (is_felm_zero(x)) return logT[0];
    if (memcmp(x, y, NBITS_TO_NBYTES(NBITS_FIELD)) == 0) return logT[1];
    fpcopy(y, sum);
    fpneg(sum);
    fpcorrection(sum);
    if (memcmp(x, sum, NBITS_TO_NBYTES(NBITS_FIELD)) == 0) return logT[2];    
    for (int j = 2; j < W_2; ++j) {
        for (int i = 0; i < (1<<(j-1)); ++i) {
            if ((i % 2) == 0) 
                fpmul_mont(y, Texp[(1<<(j-2)) + (i/2) - 1], prods[(1<<(j-2)) + (i/2) - 1]);
            fpcopy(y, sum);
            for (int k = 0; k <= j-2; ++k) {
                if (((i>>(j-k-2)) % 2) == 0) 
                    fpadd(sum, prods[(1<<k) + (i >> (j-k-1)) - 1], sum);
                else 
                    fpsub(sum, prods[(1<<k) + (i >> (j-k-1)) - 1], sum);
            }
            fpcorrection(sum);
            if (memcmp(x, sum, NBITS_TO_NBYTES(NBITS_FIELD)) == 0) {
                return logT[(1<<j)+i-1];
            }
            fpneg(sum);
            fpcorrection(sum);
            if (memcmp(x, sum, NBITS_TO_NBYTES(NBITS_FIELD)) == 0) {
                return logT[(1<<(j+1))-i-1-1];
            }
        }
    }
    return 0;
}
#include <stdio.h>
// Input: h =[x,y] = rho^D in G_{ell=2,w} generated by rho, i.e., |h| <= 2^w
// Output: The signed digit D in {-ell^(w-1), ..., ell^(w-1)}
int ord2w_dloghyb(const felm_t *h, const int *logT, const felm_t *Texp, const felm_t *G)
{
    int k = 0, d = 0, index = 0, ord = 0, tmp = 0, w = W_2, w2 = w - W_2_1, i_j = 0, t;
    uint8_t inv = 0, flag = 0;
    f2elm_t H[W_2_1+1] = {0}; // Size of H should be max of {W_2_1+1, W_2 - W_2_1}
    felm_t one = {0};

    fpcopy((digit_t*)&Montgomery_one, one);    
    fp2copy(h, (felm_t*)&H[0]);
    fpcorrection(H[0][0]);
    fpcorrection(H[0][1]);

    for (int i = 1; i <= w2; ++i) {
        if (!is_felm_zero(H[0][1])) { // check if first compressed Fp2 element in H is NOT the identity
            for (int j = k; j >= 0; j--) fp2copy(H[j], H[j+1]);
            sqr_Fp2_cycl_proj(H[0]);
            k++;
        } else {
            break;
        }        
    }

    fpcorrection(H[0][1]);
    if (is_felm_zero(H[0][1]) && k <= W_2_1) return ord2w_dlog(H[k], logT, Texp) << w2;

    if (!is_felm_zero(H[0][1])) {        
        d = mod(ord2w_dlog(H[0], logT, Texp), (1 << W_2_1));
        index = 0;
    } else {        
        d = mod(ord2w_dlog(&H[W_2_1][0], logT, Texp), (1 << W_2_1));
        index = W_2_1;
    }

    t = highest_t(d);
    ord = W_2_1 - t;
    tmp = ((d >> (W_2_1-ord))-1) >> 1;
    i_j = reverse_bits(tmp,ord-1);
    fpcorrection(H[0][0]);
    if (is_felm_zero(H[0][0])) { // check if compressed Fp2 element H[0] is -1
        fpneg(one);
        fpcorrection(one);
        if ( ((memcmp(G[0],&Montgomery_one,NBITS_TO_NBYTES(NBITS_FIELD)) == 0) && (memcmp(H[1][0],H[1][1],NBITS_TO_NBYTES(NBITS_FIELD)) != 0)) ||
             ((memcmp(G[0],one,NBITS_TO_NBYTES(NBITS_FIELD)) == 0) && (memcmp(H[1][0],H[1][1],NBITS_TO_NBYTES(NBITS_FIELD)) == 0))) { // check if G[0] != H[1]
            for (int i = 0; i <= k; ++i) inv_Fp2_cycl_proj(H[i]);
            inv = 1;
        }
    } else {
        if (i_j >= (1 << (ord-2))) {
            i_j = (1 << (ord-1)) - i_j - 1; 
            d = (1 << (W_2_1)) - d;
            for (int i = 0; i <= k; ++i) inv_Fp2_cycl_proj(H[i]);             
            inv = 1;   
        }
    } 
    d <<= w2;
    for (int j = index+1; j <= k; ++j) {
        i_j <<= 1;
        d >>= 1;

        if ((j-index+ord-3) < 0) {
            fpmul_mont(G[0], H[j][1], one);
        } else {            
            fpmul_mont(G[(1 << (j-index+ord-2)) - (1 << (j-index+ord-3)) + (i_j >> 1)], H[j][1], one);
        }
        fpcorrection(one);
        if (memcmp(H[j][0], one, NBITS_TO_NBYTES(NBITS_FIELD)) != 0) {
            d += 1 << (w-1);
            i_j++;
            flag = 1;
        }
    }

    if (d > (1 << (w-1))) d = (1 << w) - d;
    if (inv ^ flag) d = -d;
    
    return d;
}


void Traverse_w_div_e_torus(const felm_t *r, int j, int k, int z, const unsigned int *P, const felm_t *CT, int *D, int Dlen, int ellw, int w)
{// Traverse a Pohlig-Hellman optimal strategy to solve a discrete log in a group of order 2^e
 // The leaves of the tree will be used to recover the signed digits which are numbers from +/-{0,1... Ceil((2^w-1)/2)}
 // Assume the integer w divides the exponent e
    felm_t rp[2] = {0}, alpha = {0};
    
    if (z > 1) {
        int t = P[z];
        fp2copy(r, rp);
        for (int i = 0; i < (z-t)*w; i++) sqr_Fp2_cycl_proj(rp);
        
        Traverse_w_div_e_torus(rp, j + (z - t), k, t, P, CT, D, Dlen, ellw, w);  
        
        fp2copy(r, rp);
        for (int h = k; h < k + t; h++) {
            if (D[h] != 0) {
                if(D[h] < 0) {
                    fpcopy(CT[(j + h)*(ellw/2) - (D[h]+1)], alpha);
                    fpneg(alpha);
                    mulmixed_montproj(rp,  alpha, rp);                    
                } else {
                    mulmixed_montproj(rp,  CT[(j + h)*(ellw/2) + (D[h]-1)], rp);
                }
            }
        }
        Traverse_w_div_e_torus(rp, j, k + t, z - t, P, CT, D, Dlen, ellw, w);
    } else {
        fpcorrection((digit_t*)&r[0]);
        fpcorrection((digit_t*)&r[1]);

        D[k] = ord2w_dloghyb(r, (const int *)&ph2_Log, (const felm_t *)&ph2_Texp, (const felm_t *)&ph2_G);           
    }
}

#endif // Closing ELL2_TORUS

#if defined(ELL3_FULL_SIGNED)

void Traverse_w_div_e_fullsigned(const f2elm_t r, int j, int k, int z, const unsigned int *P, const felm_t *CT, int *D, 
                                 int Dlen, int ellw, int w)
{// Traverse a Pohlig-Hellman optimal strategy to solve a discrete log in a group of order ell^e
 // The leaves of the tree will be used to recover the signed digits which are numbers from +/-{0,1... Ceil((ell^w-1)/2)}
 // Assume the integer w divides the exponent e
    f2elm_t rp = {0}, alpha = {0};
    
    if (z > 1) {
        int t = P[z];
        fp2copy(r, rp);
        for (int i = 0; i < z-t; i++) {
            if ((ellw & 1) == 0) {
                for (int ii = 0; ii < w; ii++)
                    sqr_Fp2_cycl(rp, (digit_t*)&Montgomery_one);
            } else {
                for (int ii = 0; ii < w; ii++)
                    cube_Fp2_cycl(rp, (digit_t*)&Montgomery_one);
            }
        }
        
        Traverse_w_div_e_fullsigned(rp, j + (z - t), k, t, P, CT, D, Dlen, ellw, w);  
        
        fp2copy(r, rp);
        for (int h = k; h < k + t; h++) {
            if (D[h] != 0) {
                if(D[h] < 0) {
                    fp2copy(CT + 2*((j + h)*(ellw/2) + (-D[h]-1)), alpha);
                    fpneg(alpha[1]);
                    fp2mul_mont(rp, alpha, rp);
                } else {
                    fp2mul_mont(rp, CT + 2*((j + h)*(ellw/2) + (D[h]-1)), rp);
                }   
            }
        }
        Traverse_w_div_e_fullsigned(rp, j, k + t, z - t, P, CT, D, Dlen, ellw, w);
    } else {     
        fp2copy(r, rp);
        fp2correction(rp);

        if (is_felm_zero(rp[1]) && memcmp(rp[0],&Montgomery_one,NBITS_TO_NBYTES(NBITS_FIELD)) == 0) {
            D[k] = 0;
        } else {
            for (int t = 1; t <= ellw/2; t++) {
                if (memcmp(rp, CT[2*((Dlen - 1)*(ellw/2) + (t-1))], 2*NBITS_TO_NBYTES(NBITS_FIELD)) == 0) {
                    D[k] = -t;
                    break;
                } else {                    
                    fp2copy(CT + 2*((Dlen - 1)*(ellw/2) + (t-1)), alpha);
                    fpneg(alpha[1]);
                    fpcorrection(alpha[1]);
                    if (memcmp(rp, alpha, 2*NBITS_TO_NBYTES(NBITS_FIELD)) == 0) {
                        D[k] = t;
                        break;
                    }
                }               
            }
        }
    }
}


void Traverse_w_notdiv_e_fullsigned(const f2elm_t r, int j, int k, int z, const unsigned int *P, const felm_t *CT1, const felm_t *CT2, 
                                    int *D, int Dlen, int ell, int ellw, int ell_emodw, int w, int e)
{ // Traverse a Pohlig-Hellman optimal strategy to solve a discrete log in a group of order ell^e
 // Leaves are used to recover the digits which are numbers from 0 to ell^w-1 except by the last leaf that gives a digit between 0 and ell^(e mod w)
 // Assume w does not divide the exponent e
    f2elm_t rp = {0}, alpha = {0};            
    
    if (z > 1) {
        int t = P[z], goleft;
        fp2copy(r, rp);
        
        goleft = (j > 0) ? w*(z-t) : (e % w) + w*(z-t-1);
        for (int i = 0; i < goleft; i++) {
            if ((ell & 1) == 0)
                sqr_Fp2_cycl(rp, (digit_t*)&Montgomery_one);
            else
                cube_Fp2_cycl(rp, (digit_t*)&Montgomery_one);
        }

        Traverse_w_notdiv_e_fullsigned(rp, j + (z - t), k, t, P, CT1, CT2, D, Dlen, ell, ellw, ell_emodw, w, e);  
        
        fp2copy(r, rp);
        for (int h = k; h < k + t; h++) {
            if (D[h] != 0) {
                if (j > 0) {
                    if (D[h] < 0) {
                        fp2copy(CT2 + 2*(j + h)*(ellw/2)+2*(-D[h]-1), alpha);
                        fpneg(alpha[1]);                        
                        fp2mul_mont(rp, alpha, rp);
                    } else {
                        fp2mul_mont(rp, CT2 + 2*((j + h)*(ellw/2) + (D[h]-1)), rp);
                    }
                } else {
                    if (D[h] < 0) {
                        fp2copy(CT1 + 2*((j + h)*(ellw/2) + (-D[h]-1)), alpha);
                        fpneg(alpha[1]);
                        fp2mul_mont(rp, alpha, rp);                
                    } else {
                        fp2mul_mont(rp, CT1 + 2*((j + h)*(ellw/2) + (D[h]-1)), rp);
                    }
                }
            }             
        }
        
        Traverse_w_notdiv_e_fullsigned(rp, j, k + t, z - t, P, CT1, CT2, D, Dlen, ell, ellw, ell_emodw, w, e);
    } else {
        fp2copy(r, rp);
        fp2correction(rp);    

        if (is_felm_zero(rp[1]) && memcmp(rp[0],&Montgomery_one,NBITS_TO_NBYTES(NBITS_FIELD)) == 0) {
            D[k] = 0;              
        } else {            
            if (!(j == 0 && k == Dlen - 1)) {
                for (int t = 1; t <= (ellw/2); t++) {
                    if (memcmp(CT2[2*(ellw/2)*(Dlen-1) + 2*(t-1)], rp, 2*NBITS_TO_NBYTES(NBITS_FIELD)) == 0) {
                        D[k] = -t;
                        break;             
                    } else {
                        fp2copy(CT2 + 2*((ellw/2)*(Dlen-1) + (t-1)), alpha);
                        fpneg(alpha[1]);
                        fpcorrection(alpha[1]);
                        if (memcmp(rp, alpha, 2*NBITS_TO_NBYTES(NBITS_FIELD)) == 0) {
                            D[k] = t;
                            break;
                        }
                    }                           
                }
            } else {            
                for (int t = 1; t <= ell_emodw/2; t++) {     
                    if (memcmp(CT1[2*(ellw/2)*(Dlen - 1) + 2*(t-1)], rp, 2*NBITS_TO_NBYTES(NBITS_FIELD)) == 0) { 
                        D[k] = -t;
                        break;                
                    } else {
                        fp2copy(CT1 + 2*((ellw/2)*(Dlen-1) + (t-1)), alpha);
                        fpneg(alpha[1]);
                        fpcorrection(alpha[1]);
                        if (memcmp(rp, alpha, 2*NBITS_TO_NBYTES(NBITS_FIELD)) == 0) {
                            D[k] = t;
                            break;
                        }                        
                    }

                }
            }   
        }
    }
}
#endif  //Closing ELL3_FULL_SIGNED

#endif  // Closing COMPRESSED_TABLES


void solve_dlog(const f2elm_t r, int *D, digit_t* d, int ell)
{ // Computes the discrete log of input r = g^d where g = e(P,Q)^ell^e, and P,Q are torsion generators in the initial curve
  // Return the integer d  
    if (ell == 2) {
        felm_t rproj[2];
        toproj(r, rproj);  
        Traverse_w_div_e_torus(rproj, 0, 0, PLEN_2 - 1, ph2_path, (const felm_t *)&ph2_CT, D, DLEN_2, ELL2_W, W_2);

        from_base(D, d, DLEN_2, ELL2_W);
    } else if (ell == 3) {
        #if (OBOB_EXPON % W_3 == 0)
            Traverse_w_div_e_fullsigned(r, 0, 0, PLEN_3 - 1, ph3_path, (const felm_t *)&ph3_T, D, DLEN_3, ELL3_W, W_3);
        #else          
            Traverse_w_notdiv_e_fullsigned(r, 0, 0, PLEN_3 - 1, ph3_path, (const felm_t *)&ph3_T1, (const felm_t *)&ph3_T2, D, DLEN_3, ell, ELL3_W, ELL3_EMODW, W_3, OBOB_EXPON);                    
        #endif     
        from_base(D, d, DLEN_3, ELL3_W);
    }    
}


