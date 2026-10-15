import { updateConfig } from './config-sync';

// 主键的 keysym → server 侧解析器认的名字（见 server/src/autotype/auto_typer.cpp 的 ParseMainKey）。
// 认不出来的键一律不写进配置，避免录出一个 server 解析失败、表面启用其实不触发的热键。
const namedKeys: Record<string, string> = {
  ' ': 'space',
  Enter: 'enter',
  Tab: 'tab',
  Escape: 'escape',
  Backspace: 'backspace',
  Delete: 'delete',
  Insert: 'insert',
  Home: 'home',
  End: 'end',
  PageUp: 'pageup',
  PageDown: 'pagedown',
  ArrowUp: 'up',
  ArrowDown: 'down',
  ArrowLeft: 'left',
  ArrowRight: 'right',
  '-': 'minus',
  '=': 'equal',
  ',': 'comma',
  '.': 'period',
  '/': 'slash',
  '\\': 'backslash',
  ';': 'semicolon',
  "'": 'quote',
  '`': 'backquote',
  '[': 'bracketleft',
  ']': 'bracketright'
};

function mainKeyName(event: KeyboardEvent): string | null {
  if (/^[a-zA-Z]$/.test(event.key)) {
    return event.key.toLowerCase();
  }
  if (/^[0-9]$/.test(event.key)) {
    return event.key;
  }
  if (/^F([1-9]|1[0-9]|2[0-4])$/.test(event.key)) {
    return event.key.toLowerCase();
  }
  return namedKeys[event.key] ?? null;
}

// 组合键 → "ctrl+shift+alt+v" 形式的配置值。没有修饰键或没有主键时返回 null：
// 那种热键会把普通输入整个吃掉，不能让它被录进配置。
function comboFromEvent(event: KeyboardEvent): string | null {
  const modifiers: string[] = [];
  if (event.ctrlKey) modifiers.push('ctrl');
  if (event.shiftKey) modifiers.push('shift');
  if (event.altKey) modifiers.push('alt');
  if (event.metaKey) modifiers.push('win');
  const key = mainKeyName(event);
  if (!key || modifiers.length === 0) {
    return null;
  }
  return [...modifiers, key].join('+');
}

export function applyShortcutConfig(config: {
  switch_language_shift?: boolean;
  switch_language_ctrl?: boolean;
  switch_language_ctrl_alt_space?: boolean;
  toggle_character_set_ctrl_shift_f?: boolean;
  trilingual_cycle?: boolean;
} | undefined): void {
  if (!config) {
    return;
  }
  const shift = document.getElementById('switchLanguageShiftCheckbox') as HTMLInputElement | null;
  const ctrl = document.getElementById('switchLanguageCtrlCheckbox') as HTMLInputElement | null;
  const ctrlAltSpace = document.getElementById('switchLanguageCtrlAltSpaceCheckbox') as HTMLInputElement | null;
  const characterSet = document.getElementById('characterSetShortcutCheckbox') as HTMLInputElement | null;
  const trilingualCycle = document.getElementById('trilingualCycleCheckbox') as HTMLInputElement | null;
  if (characterSet && typeof config.toggle_character_set_ctrl_shift_f === 'boolean') {
    characterSet.checked = config.toggle_character_set_ctrl_shift_f;
  }
  if (shift && typeof config.switch_language_shift === 'boolean') {
    shift.checked = config.switch_language_shift;
  }
  if (ctrl && typeof config.switch_language_ctrl === 'boolean') {
    ctrl.checked = config.switch_language_ctrl;
  }
  if (ctrlAltSpace && typeof config.switch_language_ctrl_alt_space === 'boolean') {
    ctrlAltSpace.checked = config.switch_language_ctrl_alt_space;
  }
  if (trilingualCycle && typeof config.trilingual_cycle === 'boolean') {
    trilingualCycle.checked = config.trilingual_cycle;
  }
}

// 模拟输入（[autotype] 段）。热键输入框是只读的，值只能由录制产生，
// 因此这里不需要（也不能）校验文本——server 侧解析失败会按未配置处理。
export function applyAutoTypeConfig(config:
  | {
      enabled?: boolean;
      hotkey?: string;
      text?: string;
      send_enter?: boolean;
      char_delay_ms?: number;
    }
  | undefined): void {
  if (!config) {
    return;
  }
  const enabled = document.getElementById('autoTypeEnabledCheckbox') as HTMLInputElement | null;
  const hotkey = document.getElementById('autoTypeHotkeyInput') as HTMLInputElement | null;
  const text = document.getElementById('autoTypeTextInput') as HTMLTextAreaElement | null;
  const charDelay = document.getElementById('autoTypeCharDelayInput') as HTMLInputElement | null;
  const sendEnter = document.getElementById('autoTypeSendEnterCheckbox') as HTMLInputElement | null;
  if (enabled && typeof config.enabled === 'boolean') {
    enabled.checked = config.enabled;
  }
  if (hotkey && typeof config.hotkey === 'string') {
    hotkey.value = config.hotkey;
  }
  if (text && typeof config.text === 'string') {
    text.value = config.text;
  }
  if (charDelay && typeof config.char_delay_ms === 'number') {
    charDelay.value = String(config.char_delay_ms);
  }
  if (sendEnter && typeof config.send_enter === 'boolean') {
    sendEnter.checked = config.send_enter;
  }
}

export function setupAutoType(): void {
  const enabled = document.getElementById('autoTypeEnabledCheckbox') as HTMLInputElement | null;
  enabled?.addEventListener('change', () => {
    updateConfig('autotype.enabled', enabled.checked);
  });

  const hotkey = document.getElementById('autoTypeHotkeyInput') as HTMLInputElement | null;
  if (hotkey) {
    // 只读 + 按键录制：keydown 在捕获阶段处理并一律 preventDefault，
    // 否则按下组合键时页面会顺带触发浏览器自身的快捷键。
    hotkey.addEventListener('keydown', (event: KeyboardEvent) => {
      event.preventDefault();
      event.stopPropagation();
      if (event.key === 'Escape') {
        hotkey.value = '';
        updateConfig('autotype.hotkey', '');
        hotkey.blur();
        return;
      }
      const combo = comboFromEvent(event);
      if (!combo) {
        return;
      }
      hotkey.value = combo;
      updateConfig('autotype.hotkey', combo);
      hotkey.blur();
    });
    // 只读输入框拿不到焦点就录不了键，所以点一下就主动聚焦。
    hotkey.addEventListener('mousedown', () => hotkey.focus());
  }

  const hotkeyClear = document.getElementById('autoTypeHotkeyClearButton') as HTMLButtonElement | null;
  hotkeyClear?.addEventListener('click', () => {
    if (hotkey) {
      hotkey.value = '';
    }
    updateConfig('autotype.hotkey', '');
  });

  const text = document.getElementById('autoTypeTextInput') as HTMLTextAreaElement | null;
  if (text) {
    // textarea 的 change 只在失焦时触发，而用户完全可能打完预置文本就直接切去按热键——
    // 那样这次编辑根本没落盘，Server 读到的还是旧文本。所以用 input 事件配合防抖：
    // 停止输入 400ms 后保存，既不用等失焦，也不会每敲一个字就写一次配置。
    let textSaveTimer: ReturnType<typeof setTimeout> | undefined;
    const flushText = () => {
      if (textSaveTimer !== undefined) {
        clearTimeout(textSaveTimer);
        textSaveTimer = undefined;
      }
      updateConfig('autotype.text', text.value);
    };
    text.addEventListener('input', () => {
      if (textSaveTimer !== undefined) {
        clearTimeout(textSaveTimer);
      }
      textSaveTimer = setTimeout(() => {
        textSaveTimer = undefined;
        updateConfig('autotype.text', text.value);
      }, 400);
    });
    // 失焦时立刻补一次，避免"打完最后一个字立刻点走"落在这个 400ms 窗口里丢掉。
    text.addEventListener('blur', flushText);
  }

  const charDelay = document.getElementById('autoTypeCharDelayInput') as HTMLInputElement | null;
  charDelay?.addEventListener('change', () => {
    const parsed = Number.parseInt(charDelay.value, 10);
    // server 侧的 setter 会拒绝越界值；这里先把输入本身拉回合法区间，避免出现"改了但没保存"的错觉。
    const clamped = Number.isFinite(parsed) ? Math.min(5000, Math.max(0, parsed)) : 30;
    charDelay.value = String(clamped);
    updateConfig('autotype.char_delay_ms', clamped);
  });

  const sendEnter = document.getElementById('autoTypeSendEnterCheckbox') as HTMLInputElement | null;
  sendEnter?.addEventListener('change', () => {
    updateConfig('autotype.send_enter', sendEnter.checked);
  });
}

export function setupShortcut(): void {
  const characterSet = document.getElementById('characterSetShortcutCheckbox') as HTMLInputElement | null;
  characterSet?.addEventListener('change', () => {
    updateConfig('keybindings.toggle_character_set_ctrl_shift_f', characterSet.checked);
  });
  const trilingualCycle = document.getElementById('trilingualCycleCheckbox') as HTMLInputElement | null;
  trilingualCycle?.addEventListener('change', () => {
    updateConfig('keybindings.trilingual_cycle', trilingualCycle.checked);
  });
  const mapping: Record<string, string> = {
    shift: 'keybindings.switch_language_shift',
    ctrl: 'keybindings.switch_language_ctrl',
    'ctrl-alt-space': 'keybindings.switch_language_ctrl_alt_space'
  };
  document.querySelectorAll<HTMLInputElement>('input[name="switch-language"]').forEach((checkbox) => {
    checkbox.addEventListener('change', () => {
      const path = mapping[checkbox.value];
      if (path) {
        updateConfig(path, checkbox.checked);
      }
    });
  });
}
