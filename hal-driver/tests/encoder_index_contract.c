/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include "encoder_index.h"
int main(void) {
    sn_index_state s={0};
    assert(!sn_index_hit(&s,100));
    for (unsigned request=1;request<=600;request++) {
        uint8_t tag=(uint8_t)request;
        sn_index_command(&s,1,tag);assert(s.armed && !s.hit);
        assert(sn_index_hit(&s,(int32_t)(1000+request)));
        for (int retry=0;retry<20;retry++) {
            sn_index_command(&s,1,tag);assert(s.hit && !s.armed);
            assert(!sn_index_hit(&s,9999));assert(s.count==(int32_t)(1000+request));
        }
        sn_index_command(&s,0,(uint8_t)(tag-1));assert(s.hit); /* stale clear */
        sn_index_command(&s,0,tag);assert(!s.hit && !s.armed);
        sn_index_command(&s,1,tag);assert(!s.armed); /* delayed duplicate request */
    }
    sn_index_command(&s,1,(uint8_t)(s.tag+1));assert(s.armed);
    sn_index_command(&s,0,s.tag);assert(!s.armed && !s.hit); /* cancel */
    assert(!sn_index_hit(&s,1));
    sn_index_command(&s,1,(uint8_t)(s.tag-1));assert(!s.armed); /* old generation */
    sn_index_state restarted={0};
    sn_index_command(&restarted,1,240);assert(restarted.armed && restarted.tag==240);
    return 0;
}
