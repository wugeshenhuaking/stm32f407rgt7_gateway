#ifndef CO_DRIVER_CUSTOM_H
#define CO_DRIVER_CUSTOM_H

/*
 * Override selected CANopenNode stack configuration for this target port.
 *
 * We enable NMT master support here instead of modifying CANopenNode sources
 * directly, so the stack implementation stays upstream-compatible and the
 * application can use the official CO_NMT_sendCommand() API.
 */
#undef CO_CONFIG_NMT
#define CO_CONFIG_NMT (CO_CONFIG_GLOBAL_FLAG_CALLBACK_PRE | \
                       CO_CONFIG_GLOBAL_FLAG_TIMERNEXT | \
                       CO_CONFIG_NMT_MASTER)

#endif /* CO_DRIVER_CUSTOM_H */
