#include "core/core.h"

#include <kuroko/kuroko.h>
#include <kuroko/util.h>
#include <kuroko/scanner.h>

static KrkInstance * module;
static KrkInstance * mainModule;
static tic_mem * machine;

static void closeKuroko(tic_mem* tic) {
    krk_freeVM();
}

struct ColorKey {
    u8 colors[TIC_PALETTE_SIZE];
    int count;
};

static int unpackColorkey(void * context, const KrkValue * items, size_t count) {
    struct ColorKey * colorkey = context;

    for (size_t i = 0; i < count; ++i) {
        if (colorkey->count >= TIC_PALETTE_SIZE) {
            krk_runtimeError(vm.exceptions->valueError, "too many values to unpack; expected %d", TIC_PALETTE_SIZE);
            return 1;
        }

        if (!IS_INTEGER(items[i])) {
            krk_runtimeError(vm.exceptions->typeError, "expected int, not %T", items[i]);
            return 1;
        }

        colorkey->colors[colorkey->count] = AS_INTEGER(items[i]);
        colorkey->count++;
    }

    return 0;
}

static int handleColorKey(KrkValue colors, struct ColorKey * colorkey) {
    if (IS_INTEGER(colors)) {
        colorkey->colors[0] = AS_INTEGER(colors);
        colorkey->count = 1;
        return 1;
    }

    if (IS_NONE(colors)) return 1;
    if (krk_unpackIterable(colors, colorkey, unpackColorkey)) return 0;

    return 1;
}

KRK_Function(print) {
    const char * text;
    int x = 0;
    int y = 0;
    int color = 15;
    int fixed = 0;
    int scale = 1;
    int alt = 0;

    if (!krk_parseArgs("s|iiipip", (const char*[]){"text","x","y","color","fixed","scale","alt"},
        &text, &x, &y, &color, &fixed, &scale, &alt)) return NONE_VAL();

    int width = tic_api_print(machine, text, x, y, color, fixed, scale, alt);

    return INTEGER_VAL(width);
}

KRK_Function(cls) {
    int color;

    if (!krk_parseArgs("i",(const char*[]){"color"},&color)) return NONE_VAL();

    tic_api_cls(machine, color);

    return NONE_VAL();
}

KRK_Function(pix) {
    int x, y, color = 0, has_color = 0;

    if (!krk_parseArgs("ii|i?", (const char*[]){"x","y","color"},
        &x, &y, &has_color, &color)) return NONE_VAL();

    int result = tic_api_pix(machine, x, y, color, !has_color);

    return INTEGER_VAL(result);
}

KRK_Function(line) {
    float x0, y0, x1, y1;
    int color;

    if (!krk_parseArgs("ffffi", (const char*[]){"x0","y0","x1","y1","color"},
        &x0, &y0, &x1, &y1, &color)) return NONE_VAL();

    tic_api_line(machine, x0, y0, x1, y1, color);

    return NONE_VAL();
}

KRK_Function(rect) {
    int x, y, w, h, color;

    if (!krk_parseArgs("iiiii", (const char*[]){"x","y","w","h","color"},
        &x, &y, &w, &h, &color)) return NONE_VAL();

    tic_api_rect(machine, x, y, w, h, color);

    return NONE_VAL();
}

KRK_Function(rectb) {
    int x, y, w, h, color;

    if (!krk_parseArgs("iiiii", (const char*[]){"x","y","w","h","color"},
        &x, &y, &w, &h, &color)) return NONE_VAL();

    tic_api_rectb(machine, x, y, w, h, color);

    return NONE_VAL();
}

KRK_Function(spr) {
    int index, x, y;
    KrkValue colors = NONE_VAL();
    int scale = 1;
    int flip = tic_no_flip;
    int rotate = tic_no_rotate;
    int w = 1;
    int h = 1;

    if (!krk_parseArgs("iii|Viiiii",(const char*[]){"index","x","y","colorkey","scale","flip","rotate","w","h"},
        &index, &x, &y, &colors, &scale, &flip, &rotate, &w, &h)) return NONE_VAL();

    struct ColorKey colorkey = {0};
    if (!handleColorKey(colors, &colorkey)) return NONE_VAL();

    tic_api_spr(machine, index, x, y, w, h, colorkey.colors, colorkey.count, scale, flip, rotate);

    return NONE_VAL();
}

KRK_Function(btn) {
    int id;

    if (!krk_parseArgs("i",(const char*[]){"id"},&id)) return NONE_VAL();

    int result = tic_api_btn(machine, id);

    return BOOLEAN_VAL(result);
}

KRK_Function(btnp) {
    int id, hold=-1, period=-1;

    if (!krk_parseArgs("i|ii", (const char*[]){"id","hold","period"},
        &id, &hold, &period)) return NONE_VAL();

    int result = tic_api_btnp(machine, id, hold, period);

    return BOOLEAN_VAL(result);
}

struct VolumeList {
    int volumes[2];
    int count;
};

static int unpackVolume(void * context, const KrkValue * items, size_t count) {
    struct VolumeList * volumes = context;
    for (size_t i = 0; i < count; ++i) {
        if (volumes->count >= 2) {
            krk_runtimeError(vm.exceptions->valueError, "too many values to unpack; expected %d", 2);
            return 1;
        }
        if (!IS_INTEGER(items[i])) {
            krk_runtimeError(vm.exceptions->typeError, "expected int, not %T", items[i]);
            return 1;
        }

        volumes->volumes[volumes->count] = AS_INTEGER(items[i]);
        volumes->count++;
    }

    return 0;
}

static int handleVolume(KrkValue from, s32 *left, s32 *right) {
    if (IS_INTEGER(from)) {
        *left = *right = AS_INTEGER(from) & 0xF;
        return 1;
    }
    struct VolumeList volumes = {0};
    if (krk_unpackIterable(from, &volumes, unpackVolume)) return 0;
    return 1;
}

KRK_Function(sfx) {
    int id;
    KrkValue note = INTEGER_VAL(-1);
    int duration = -1, channel = 0;
    KrkValue volume = INTEGER_VAL(15);
    int speed = 0;

    if (!krk_parseArgs("i|ViiVi", (const char*[]){"id","note","duration","channel","volume","speed"},
        &id, &note, &duration, &channel, &volume, &speed)) return NONE_VAL();

    s32 note_val, octave;

    if (IS_STRING(note)) {
        if (!tic_tool_parse_note(AS_CSTRING(note), &note_val, &octave)) {
            return krk_runtimeError(vm.exceptions->valueError, "invalid note name");
        }
    } else if (IS_INTEGER(note)) {
        note_val = AS_INTEGER(note) % NOTES;
        octave   = AS_INTEGER(note) / NOTES;
    } else {
        return krk_runtimeError(vm.exceptions->typeError, "expected str or int, not %T", note);
    }

    if (channel < 0 || channel >= TIC_SOUND_CHANNELS) return krk_runtimeError(vm.exceptions->valueError, "invalid channel");

    s32 left, right;

    if (!handleVolume(volume, &left, &right)) return NONE_VAL();

    tic_api_sfx(machine, id, note_val, octave, duration, channel, left, right, speed);

    return NONE_VAL();
}

static void remap_callback(void* data, s32 x, s32 y, RemapResult* result) {
    KrkValue * callback = data;

    /* Short circuit exceptions */
    if (krk_currentThread.flags & KRK_THREAD_HAS_EXCEPTION) return;

    /* callback(x, y) -> tuple[int, int, int] */
    krk_push(*callback);
    krk_push(INTEGER_VAL(x));
    krk_push(INTEGER_VAL(y));
    krk_push(krk_callStack(2));

    /* That may also have failed. */
    if (krk_currentThread.flags & KRK_THREAD_HAS_EXCEPTION) return;

    if (!IS_TUPLE(krk_peek(0))) {
        krk_runtimeError(vm.exceptions->typeError, "expected tuple of int, not %T", krk_peek(0));
        return;
    }

    KrkTuple * values = AS_tuple(krk_peek(0));

    if (values->values.count != 3 ||
        !IS_INTEGER(values->values.values[0]) ||
        !IS_INTEGER(values->values.values[1]) ||
        !IS_INTEGER(values->values.values[2])) {
        krk_runtimeError(vm.exceptions->typeError, "expected tuple of int, not %T", krk_peek(0));
        return;
    }

    result->index  = (u8)AS_INTEGER(values->values.values[0]);
    result->flip   =     AS_INTEGER(values->values.values[1]);
    result->rotate =     AS_INTEGER(values->values.values[2]);

    krk_pop();
}

KRK_Function(map) {
    int x = 0, y = 0, w = 30, h = 17, sx = 0, sy = 0;
    KrkValue colors = NONE_VAL();
    int scale = 1;
    KrkValue remap = NONE_VAL();

    if (!krk_parseArgs("|iiiiiiViV",(const char*[]){"x","y","w","h","sx","sy","colorkey","scale","remap"},
        &x, &y, &w, &h, &sx, &sy, &colors, &scale, &remap));

    struct ColorKey colorkey = {0};
    if (!handleColorKey(colors, &colorkey)) return NONE_VAL();

    tic_api_map(machine, x, y, w, h, sx, sy, colorkey.colors, colorkey.count, scale, IS_NONE(remap) ? NULL : remap_callback, IS_NONE(remap) ? NULL : &remap);

    return NONE_VAL();
}

KRK_Function(mget) {
    int x, y;

    if (!krk_parseArgs("ii", (const char*[]){"x","y"},
        &x, &y)) return NONE_VAL();

    int result = tic_api_mget(machine, x, y);

    return INTEGER_VAL(result);
}

KRK_Function(mset) {
    int x, y, tile_id;

    if (!krk_parseArgs("iii", (const char*[]){"x","y","tile_id"},
        &x, &y, &tile_id)) return NONE_VAL();

    tic_api_mset(machine, x, y, tile_id);

    return NONE_VAL();
}

KRK_Function(peek) {
    int addr, bits=8;
    if (!krk_parseArgs("i|i", (const char*[]){"addr","bits"},
        &addr, &bits)) return NONE_VAL();

    int result = tic_api_peek(machine, addr, bits);

    return INTEGER_VAL(result);
}

KRK_Function(poke) {
    int addr, value, bits=8;
    if (!krk_parseArgs("ii|i", (const char*[]){"addr","value","bits"},
        &addr, &value, &bits)) return NONE_VAL();

    tic_api_poke(machine, addr, value, bits);

    return NONE_VAL();
}

KRK_Function(peek1) {
    int addr;
    if (!krk_parseArgs("i", (const char*[]){"addr"}, &addr)) return NONE_VAL();
    int result = tic_api_peek1(machine, addr);
    return INTEGER_VAL(result);
}

KRK_Function(peek2) {
    int addr;
    if (!krk_parseArgs("i", (const char*[]){"addr"}, &addr)) return NONE_VAL();
    int result = tic_api_peek2(machine, addr);
    return INTEGER_VAL(result);
}

KRK_Function(peek4) {
    int addr;
    if (!krk_parseArgs("i", (const char*[]){"addr"}, &addr)) return NONE_VAL();
    int result = tic_api_peek4(machine, addr);
    return INTEGER_VAL(result);
}

KRK_Function(poke1) {
    int addr, value;
    if (!krk_parseArgs("ii", (const char*[]){"addr","value"}, &addr, &value)) return NONE_VAL();
    tic_api_poke1(machine, addr, value);
    return NONE_VAL();
}

KRK_Function(poke2) {
    int addr, value;
    if (!krk_parseArgs("ii", (const char*[]){"addr","value"}, &addr, &value)) return NONE_VAL();
    tic_api_poke2(machine, addr, value);
    return NONE_VAL();
}

KRK_Function(poke4) {
    int addr, value;
    if (!krk_parseArgs("ii", (const char*[]){"addr","value"}, &addr, &value)) return NONE_VAL();
    tic_api_poke4(machine, addr, value);
    return NONE_VAL();
}

KRK_Function(memcpy) {
    int dest, source, size;
    if (!krk_parseArgs("iii", (const char*[]){"dest","source","size"}, &dest, &source, &size)) return NONE_VAL();

    tic_api_memcpy(machine, dest, source, size);

    return NONE_VAL();
}

KRK_Function(memset) {
    int dest, value, size;
    if (!krk_parseArgs("iii", (const char*[]){"dest","value","size"}, &dest, &value, &size)) return NONE_VAL();

    tic_api_memset(machine, dest, value, size);

    return NONE_VAL();
}

KRK_Function(trace) {
    const char * message;
    int color = 15;

    if (!krk_parseArgs("s|i", (const char*[]){"message","color"}, &message, &color)) return NONE_VAL();

    tic_api_trace(machine, message, color);

    return NONE_VAL();
}

KRK_Function(pmem) {
    int index, hasValue = 0, value = 0;

    if (!krk_parseArgs("i|i?", (const char*[]){"index","value"}, &index, &hasValue, &value)) return NONE_VAL();

    int result = tic_api_pmem(machine, index, value, !hasValue);

    return INTEGER_VAL(result);
}

KRK_Function(time) {
    return FLOATING_VAL(tic_api_time(machine));
}

KRK_Function(tstamp) {
    return INTEGER_VAL(tic_api_tstamp(machine));
}

KRK_Function(exit) {
    tic_api_exit(machine);
    return NONE_VAL();
}

KRK_Function(font) {
    const char * text;
    int x, y;
    unsigned char chromakey;
    int char_width, char_height;
    int fixed = 0;
    int scale = 1;
    int alt   = 0;

    if (!krk_parseArgs("siibii|pip", (const char*[]){"text","x","y","chromakey","char_width","char_height","fixed","scale","alt"},
        &text, &x, &y, &chromakey, &char_width, &char_height, &fixed, &scale, &alt)) return NONE_VAL();

    if (scale == 0) return INTEGER_VAL(0);

    int result = tic_api_font(machine, text, x, y, &chromakey, 1, char_width, char_height, fixed, scale, alt);

    return INTEGER_VAL(result);
}

KRK_Function(mouse) {
    const tic80_mouse* mouse = &((tic_core*)machine)->memory.ram->input.mouse;
    tic_point pos = tic_api_mouse(machine);

    KrkTuple * out = krk_newTuple(7);
    krk_push(OBJECT_VAL(out));

    out->values.values[out->values.count++] = FLOATING_VAL(pos.x);
    out->values.values[out->values.count++] = FLOATING_VAL(pos.y);
    out->values.values[out->values.count++] = BOOLEAN_VAL(mouse->left);
    out->values.values[out->values.count++] = BOOLEAN_VAL(mouse->right);
    out->values.values[out->values.count++] = BOOLEAN_VAL(mouse->middle);
    out->values.values[out->values.count++] = FLOATING_VAL(mouse->scrollx);
    out->values.values[out->values.count++] = FLOATING_VAL(mouse->scrolly);

    return krk_pop();
}

KRK_Function(circ) {
    int x, y, radius, color;

    if (!krk_parseArgs("iiii", (const char*[]){"x","y","radius","color"}, &x, &y, &radius, &color)) return NONE_VAL();

    tic_api_circ(machine, x, y, radius, color);

    return NONE_VAL();
}

KRK_Function(circb) {
    int x, y, radius, color;

    if (!krk_parseArgs("iiii", (const char*[]){"x","y","radius","color"}, &x, &y, &radius, &color)) return NONE_VAL();

    tic_api_circb(machine, x, y, radius, color);

    return NONE_VAL();
}

KRK_Function(elli) {
    int x, y, a, b, color;

    if (!krk_parseArgs("iiiii", (const char*[]){"x","y","a","b","color"},
        &x, &y, &a, &b, &color)) return NONE_VAL();

    tic_api_elli(machine, x, y, a, b, color);

    return NONE_VAL();
}

KRK_Function(ellib) {
    int x, y, a, b, color;

    if (!krk_parseArgs("iiiii", (const char*[]){"x","y","a","b","color"},
        &x, &y, &a, &b, &color)) return NONE_VAL();

    tic_api_ellib(machine, x, y, a, b, color);

    return NONE_VAL();
}

KRK_Function(tri) {
    float x1, y1, x2, y2, x3, y3;
    int color;

    if (!krk_parseArgs("ffffffi", (const char*[]){"x1","y1","x2","y2","x3","y3","color"},
        &x1, &y1, &x2, &y2, &x3, &y3, &color)) return NONE_VAL();

    tic_api_tri(machine, x1, y1, x2, y2, x3, y3, color);

    return NONE_VAL();
}

KRK_Function(trib) {
    float x1, y1, x2, y2, x3, y3;
    int color;

    if (!krk_parseArgs("ffffffi", (const char*[]){"x1","y1","x2","y2","x3","y3","color"},
        &x1, &y1, &x2, &y2, &x3, &y3, &color)) return NONE_VAL();

    tic_api_trib(machine, x1, y1, x2, y2, x3, y3, color);

    return NONE_VAL();
}

KRK_Function(ttri) {
    float x1, y1, x2, y2, x3, y3;
    float u1, v1, u2, v2, u3, v3;
    int textsrc = 0;
    KrkValue chromakey = NONE_VAL();
    float z1 = 0, z2 = 0, z3 = 0;

    if (!krk_parseArgs("ffffffffffff|iVfff", (const char*[]){
            "x1","y1","x2","y2","x3","y3",
            "u1","v1","u2","v2","u3","v3",
            "textsrc","chromakey","z1","z2","z3"},
            &x1, &y1, &x2, &y2, &x3, &y3,
            &u1, &v1, &u2, &v2, &u3, &v3,
            &textsrc, &chromakey,
            &z1, &z2, &z3)) return NONE_VAL();

    struct ColorKey colorkey = {0};
    if (!handleColorKey(chromakey, &colorkey)) return NONE_VAL();

    tic_api_ttri(machine,
                 x1,y1,x2,y2,x3,y3,
                 u1,v1,u2,v2,u3,v3,
                 textsrc,colorkey.colors,colorkey.count,
                 z1,z2,z3,z1!=0||z2!=0||z3!=0);

    return NONE_VAL();
}

KRK_Function(clip) {
    int x = 0, y = 0, width = TIC80_WIDTH, height = TIC80_HEIGHT;

    if (!krk_parseArgs("|iiii", (const char*[]){"x","y","width","height"},
        &x, &y, &width, &height)) return NONE_VAL();

    tic_api_clip(machine, x, y, width, height);

    return NONE_VAL();
}

KRK_Function(music) {
    int track = -1;
    int frame = -1;
    int row = -1;
    int loop = 1;
    int sustain = 1;
    int tempo = -1;
    int speed = -1;

    if (!krk_parseArgs("|iiippii", (const char*[]){"track","frame","row","loop","sustain","tempo","speed"},
        &track, &frame, &row, &loop, &sustain, &tempo, &speed)) return NONE_VAL();

    tic_api_music(machine, track, frame, row, loop, sustain, tempo, speed);

    return NONE_VAL();
}

KRK_Function(sync) {
    unsigned int mask = 0;
    int bank = 0, tocart = 0;

    if (!krk_parseArgs("|Iip", (const char*[]){"mask","bank","tocart"},
        &mask, &bank, &tocart)) return NONE_VAL();

    tic_api_sync(machine, mask, bank, tocart);

    return NONE_VAL();
}

KRK_Function(vbank) {
    int bank, hasBank = 0;

    if (!krk_parseArgs("|i?", (const char*[]){"bank"}, &hasBank, &bank)) return NONE_VAL();

    int result = ((tic_core*)machine)->state.vbank.id;

    if (hasBank) {
        tic_api_vbank(machine, bank);
    }

    return INTEGER_VAL(result);
}

KRK_Function(reset) {
    tic_api_reset(machine);
    return NONE_VAL();
}

KRK_Function(key) {
    int code = -1;
    if (!krk_parseArgs("|i", (const char*[]){"code"}, &code)) return NONE_VAL();

    int result = tic_api_key(machine, code);

    return BOOLEAN_VAL(result);
}

KRK_Function(keyp) {
    int code = -1, hold = -1, period = -1;

    if (!krk_parseArgs("|iii", (const char*[]){"code","hold","period"},
        &code, &hold, &period)) return NONE_VAL();

    int result = tic_api_keyp(machine, code, hold, period);

    return BOOLEAN_VAL(result);
}

KRK_Function(fget) {
    int sprite_id;
    unsigned char flag;

    if (!krk_parseArgs("ib", (const char*[]){"sprite_id","flag"},
        &sprite_id, &flag)) return NONE_VAL();

    int result = tic_api_fget(machine, sprite_id, flag);

    return BOOLEAN_VAL(result);
}

KRK_Function(fset) {
    int sprite_id;
    unsigned char flag;
    int _bool;

    if (!krk_parseArgs("ibp", (const char*[]){"sprite_id","flag","bool"},
        &sprite_id, &flag, &_bool)) return NONE_VAL();

    tic_api_fset(machine, sprite_id, flag, _bool);

    return NONE_VAL();
}

static uint32_t x = 123456789;
static uint32_t y = 362436069;
static uint32_t z = 521288629;
static uint32_t w = 88675123;

static uint32_t _rand(void) {
    uint32_t t;
    t = x ^ (x << 11);
    x = y; y = z; z = w;
    w = w ^ (w >> 19) ^ t ^ (t >> 8);
    return (w & UINT32_MAX);
}

KRK_Function(random) {
    double r = (double)_rand() / (double)(UINT32_MAX);
    return FLOATING_VAL(r);
}

static void handleException(tic_mem* tic) {
    tic_core * core = (tic_core*)tic;
    if (krk_currentThread.flags & KRK_THREAD_HAS_EXCEPTION) {
        /* Clear exception state */
        krk_currentThread.flags &= ~(KRK_THREAD_HAS_EXCEPTION);

        /* Set up a string builder to hold exception data. */
        struct StringBuilder sb = {0};
        KrkValue exception = krk_currentThread.currentException;
        krk_push(exception);

        /* Line feed at start because reasons */
        krk_pushStringBuilder(&sb, '\n');

        if (IS_INSTANCE(exception)) {
            /* Generate traceback */
            KrkValue tracebackEntries;
            if (krk_tableGet_fast(&AS_INSTANCE(exception)->fields, S("traceback"), &tracebackEntries)
                && IS_list(tracebackEntries) && AS_LIST(tracebackEntries)->count > 0) {
                krk_pushStringBuilderFormat(&sb, "Traceback (most recent call last):\n");
                for (size_t i = 0; i < AS_LIST(tracebackEntries)->count; ++i) {
                    if (!IS_TUPLE(AS_LIST(tracebackEntries)->values[i])) continue;
                    KrkTuple * entry = AS_TUPLE(AS_LIST(tracebackEntries)->values[i]);
                    if (entry->values.count != 2) continue;
                    if (!IS_CLOSURE(entry->values.values[0])) continue;
                    if (!IS_INTEGER(entry->values.values[1])) continue;
                    KrkClosure * closure = AS_CLOSURE(entry->values.values[0]);
                    KrkCodeObject * function = closure->function;
                    size_t instruction = AS_INTEGER(entry->values.values[1]);
                    int lineNo = (int)krk_lineNumber(&function->chunk, instruction);

                    krk_pushStringBuilderFormat(&sb, "  File \"%S\", line %d, in %S\n",
                        function->chunk.filename, lineNo, function->name);
                }
            }
        }

        /* ExceptionClass: Exception string value */
        KrkClass * type = krk_getType(exception);
        KrkValue module = NONE_VAL();
        krk_tableGet_fast(&type->methods, S("__module__"), &module);
        if (IS_STRING(module) && AS_STRING(module) != S("builtins")) {
            krk_pushStringBuilderFormat(&sb, "%S.", module);
        }
        krk_pushStringBuilderFormat(&sb, "%T", exception);

        /* Pops exception */
        KrkValue toStr = krk_callDirect(krk_getType(exception)->_tostr, 1);
        if (!IS_STRING(toStr) || AS_STRING(toStr)->length == 0) {
            krk_pushStringBuilder(&sb, '\n');
        } else {
            krk_pushStringBuilderFormat(&sb, ": %S\n", AS_STRING(toStr));
        }

        /* Write traceback to TIC */
        core->data->error(core->data->data, sb.bytes);

        /* Free space from string builder */
        krk_discardStringBuilder(&sb);
    }
}

static bool initKuroko(tic_mem* tic, const char* code)
{
    machine = tic;

    krk_initVM(KRK_GLOBAL_CLEAN_OUTPUT);

    module = krk_startModule("tic80");

    BIND_FUNC(module,random);

#define API_FUNC_DEF(name, arglist, docstring, ...) \
        KRK_DOC(BIND_FUNC(module,name), \
            "@arguments " arglist "\n\n" docstring);
        TIC_API_LIST(API_FUNC_DEF)
#undef  API_FUNC_DEF

    mainModule = krk_startModule("<main>");
    krk_interpret(code,"<main>");
    if (krk_currentThread.flags & KRK_THREAD_HAS_EXCEPTION) {
        handleException(tic);
        return false;
    }
    return true;
}


static void evalKuroko(tic_mem* tic, const char* code) {
    KrkInstance * enclosing = krk_currentThread.module;
    krk_currentThread.module = mainModule;

    krk_interpret(code, "<main>");
    handleException(tic);

    krk_currentThread.module = enclosing;
}

static void callKurokoTick(tic_mem* tic) {
    KrkValue func;
    if (!krk_tableGet_fast(&mainModule->fields, S(TIC_FN), &func)) return;
    krk_push(func);
    krk_callStack(0);
    handleException(tic);
}

static void callKurokoBoot(tic_mem* tic) {
    KrkValue func;
    if (!krk_tableGet_fast(&mainModule->fields, S(BOOT_FN), &func)) return;
    krk_push(func);
    krk_callStack(0);
    handleException(tic);
}

static void callKurokoScanline(tic_mem* tic, s32 row, void* data) {
    KrkValue func;
    if (!krk_tableGet_fast(&mainModule->fields, S(SCN_FN), &func)) return;
    krk_push(func);
    krk_push(INTEGER_VAL(row));
    krk_callStack(1);
    handleException(tic);
}

static void callKurokoBorder(tic_mem* tic, s32 row, void* data) {
    KrkValue func;
    if (!krk_tableGet_fast(&mainModule->fields, S(BDR_FN), &func)) return;
    krk_push(func);
    krk_push(INTEGER_VAL(row));
    krk_callStack(1);
    handleException(tic);
}

static void callKurokoMenu(tic_mem* tic, s32 index, void* data) {
    KrkValue func;
    if (!krk_tableGet_fast(&mainModule->fields, S(MENU_FN), &func)) return;
    krk_push(func);
    krk_push(INTEGER_VAL(index));
    krk_callStack(1);
    handleException(tic);
}

static const tic_outline_item* getKurokoOutline(const char* code, s32* size)  {
    *size = 0;
    static tic_outline_item* items = NULL;

    /* Why does no one else just use the language's own lexer? */
    KrkScanner scanner = krk_initScanner(code);

    while (1) {
        KrkToken prev = krk_scanToken(&scanner);
        if (prev.type == TOKEN_EOF || prev.type == TOKEN_ERROR) break;

        /* Everyone else is just noting functions, but let's also do classes? */
        if (prev.type == TOKEN_DEF || prev.type == TOKEN_CLASS) {
            KrkToken next = krk_scanToken(&scanner);
            if (next.type == TOKEN_IDENTIFIER) {
                items = realloc(items, (*size + 1) * sizeof(tic_outline_item));
                items[*size].pos = next.start;
                items[*size].size = (s32)(next.length);
                (*size)++;
            }
        }
    }

    return items;

}

static const char* const KurokoKeywords[] = {
    "and","class","def","else","for","if","in","import","del",
    "let","not","or","return","while","try","except","raise",
    "continue","break","as","from","elif","lambda","with","is",
    "pass","assert","yield","finally","async","await",
};


const tic_script_config KurokoSyntaxConfig = {
    .id                 = 21,
    .name               = "kuroko",
    .fileExtension      = ".krk",
    .projectComment     = "#",
    .init               = initKuroko,
    .close              = closeKuroko,
    .tick               = callKurokoTick,
    .boot               = callKurokoBoot,

    .callback           =
    {
        .scanline       = callKurokoScanline,
        .border         = callKurokoBorder,
        .menu           = callKurokoMenu,
    },

    .getOutline         = getKurokoOutline,
    .eval               = evalKuroko,

    .blockCommentStart  = NULL,
    .blockCommentEnd    = NULL,
    .blockCommentStart2 = NULL,
    .blockCommentEnd2   = NULL,
    .singleComment      = "#",
    .blockStringStart   = NULL,
    .blockStringEnd     = NULL,
    .blockEnd           = NULL,
    .stdStringStartEnd = "'\"",

    .keywords           = KurokoKeywords,
    .keywordsCount      = COUNT_OF(KurokoKeywords),
};

