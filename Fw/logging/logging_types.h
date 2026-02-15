#ifndef LOGGING_TYPES_H
#define LOGGING_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file logging_types.h
 * @brief Базовые типы логирования/захвата для сервисного интерфейса.
 */

/**
 * @brief Размер CRC32, [байт].
 */
#define LOGGING_CRC32_SIZE (4u)

/**
 * @brief Magic-слово заголовка Record (LE), [безразм.].
 */
#define LOGGING_RECORD_MAGIC (0xA55Au)

/**
 * @brief Флаги Record.
 */
typedef enum {
  LOGGING_RECORD_FLAG_NONE = 0u,       /**< Нет флагов. */
  LOGGING_RECORD_FLAG_HAS_CRC32 = (1u << 0) /**< В payload добавлен CRC32. */
} logging_record_flag_t;

/**
 * @brief Результаты операций логирования.
 */
typedef enum {
  LOGGING_RESULT_OK = 0,          /**< Операция успешна. */
  LOGGING_RESULT_INVALID_ARG = 1, /**< Некорректный аргумент. */
  LOGGING_RESULT_NO_SPACE = 2,    /**< Недостаточно места в буфере. */
  LOGGING_RESULT_NOT_ARMED = 3,   /**< Сессия не взведена. */
  LOGGING_RESULT_NOT_TRIGGERED = 4, /**< Сессия не триггерована. */
  LOGGING_RESULT_NOT_READY = 5,   /**< Данные/состояние не готовы. */
  LOGGING_RESULT_OVERFLOW = 6     /**< Переполнение/overrun. */
} logging_result_t;

/**
 * @brief Типы записей (Record type).
 */
typedef enum {
  LOGGING_RECORD_TYPE_NONE = 0,     /**< Неопределённый тип. */
  LOGGING_RECORD_TYPE_PDO = 1,      /**< Эмуляция PDO / управление. */
  LOGGING_RECORD_TYPE_CTRL = 2,     /**< Контур управления (snapshot/record). */
  LOGGING_RECORD_TYPE_ADC_RAW = 3,  /**< Сырые данные АЦП (ADC_RAW). */
  LOGGING_RECORD_TYPE_SLOW_MEAS = 4, /**< Медленные измерения (SLOW_MEAS). */
  LOGGING_RECORD_TYPE_META = 5      /**< Метаданные сессии/формата. */
} logging_record_type_t;

/**
 * @brief Состояние сессии захвата.
 */
typedef enum {
  LOGGING_CAPTURE_IDLE = 0,      /**< Сессия отсутствует. */
  LOGGING_CAPTURE_ARMED = 1,     /**< Сессия взведена (pretrigger). */
  LOGGING_CAPTURE_TRIGGERED = 2, /**< Сессия триггерована (posttrigger). */
  LOGGING_CAPTURE_STOPPED = 3    /**< Сессия завершена. */
} logging_capture_state_t;

/**
 * @brief Таймстемп записи, привязанный к PWM-домену.
 */
typedef struct {
  uint32_t pwm_period_count; /**< Номер периода PWM, [шаги]. */
  uint16_t pwm_subtick;      /**< Подтакты внутри периода PWM, [шаги]. */
  uint16_t domain_id;        /**< Идентификатор домена времени, [id]. */
} logging_timestamp_t;

/**
 * @brief Заголовок записи (Record header).
 */
typedef struct {
  uint16_t magic;       /**< Magic-слово заголовка (LOGGING_RECORD_MAGIC), [id]. */
  uint8_t type;         /**< Тип записи (logging_record_type_t), [id]. */
  uint8_t flags;        /**< Флаги записи, [битовая маска]. */
  uint16_t source_id;   /**< Идентификатор источника, [id]. */
  uint16_t payload_len; /**< Длина payload, [байт]. */
  uint16_t seq;         /**< Номер последовательности, [шаги]. */
  uint32_t pwm_period_count; /**< Номер периода PWM, [шаги]. */
  uint16_t pwm_subtick; /**< Подтакты внутри периода PWM, [шаги]. */
} logging_record_header_t;

/**
 * @brief Конфигурация сессии захвата.
 */
typedef struct {
  uint32_t pretrigger_bytes;  /**< Окно до триггера, [байт]. */
  uint32_t posttrigger_bytes; /**< Окно после триггера, [байт]. */
} logging_session_cfg_t;

/**
 * @brief Статус сессии захвата.
 */
typedef struct {
  uint32_t session_id;    /**< Идентификатор сессии, [id]. */
  logging_capture_state_t state; /**< Текущее состояние, [enum]. */
  uint32_t window_len;    /**< Длина окна захвата, [байт]. */
  uint32_t dropped_bytes; /**< Потерянные байты (drop), [байт]. */
  bool overrun;           /**< Признак переполнения, [bool]. */
  bool incomplete;        /**< Признак незавершённой сессии, [bool]. */
} logging_session_status_t;

#ifdef __cplusplus
}
#endif

#endif /* LOGGING_TYPES_H */
