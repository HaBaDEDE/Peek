(() => {
  const button = document.querySelector("#language");
  const translatable = document.querySelectorAll("[data-en][data-zh]");
  const stored = localStorage.getItem("peek-site-language");
  const preferred = navigator.language.toLowerCase().startsWith("zh") ? "zh" : "en";

  function setLanguage(language) {
    const next = language === "zh" ? "zh" : "en";
    document.documentElement.lang = next === "zh" ? "zh-CN" : "en";
    document.title = next === "zh" ? "Peek — 极小的原生 Windows 工具" : "Peek — Tiny native Windows utility";
    translatable.forEach((element) => {
      element.textContent = element.dataset[next];
    });
    button.textContent = next === "zh" ? "EN" : "中文";
    button.dataset.language = next;
    localStorage.setItem("peek-site-language", next);
  }

  button.addEventListener("click", () => {
    setLanguage(button.dataset.language === "zh" ? "en" : "zh");
  });

  setLanguage(stored || preferred);
})();
