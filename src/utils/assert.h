/**
 * @file    assert.h
 * @brief   Custom assertion handler — logs file/line and halts
 */

#ifndef ASSERT_H_
#define ASSERT_H_

#ifdef __cplusplus
extern "C" {
#endif

void assert_failed(const char *file, int line, const char *expr);

#define ASSERT(expr) \
    do { \
        if (!(expr)) { \
            assert_failed(__FILE__, __LINE__, #expr); \
        } \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* ASSERT_H_ */
