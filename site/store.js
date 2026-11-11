/* Squelchmoth store map - THE one file to edit when the Payhip products exist.
   For each product, paste its Payhip product key: the part after /b/ in the product link (https://payhip.com/b/AbCd -> "AbCd").
   With a key, every [data-buy] button for that product opens Payhip's checkout over the page (cards, PayPal, Apple Pay;
   tax handled by Payhip) and the key is emailed instantly. Without one, the button goes to the store page below. */
window.SQM_STORE = {
  store: "https://payhip.com/Squelchmoth",   // your Payhip store page (Payhip > Settings > Store URL)
  products: {
    DEMO:     "",   // free product: the installer zips, price $0 - captures the email
    CORE:     "",   // Squelchmoth                 $189.99  (upload "CORE keys (500).txt")
    COMPLETE: "",   // Squelchmoth Complete        $399.99  (upload "COMPLETE keys (500).txt")
    VOCAL:    "",   // Vocal Suite                 $149  (upload "VOCAL keys (500).txt")
    MASTER:   "",   // Mastering Suite             $99
    INSTR:    "",   // Instrument Collection       $99
    BEAT:     "",   // Beat Lab Expansion          $49
    CREATIVE: "",   // Creative FX                 $49
    COLOR:    ""    // Color & Space               $179  (upload "COLOR keys (500).txt")
  }
};

(function () {
  var S = window.SQM_STORE, any = false;
  document.querySelectorAll("[data-buy]").forEach(function (a) {
    var key = S.products[a.getAttribute("data-buy")];
    if (key) {
      any = true;
      a.href = "https://payhip.com/b/" + key;
      a.classList.add("payhip-buy-button");
      a.setAttribute("data-product", key);
      a.setAttribute("data-theme", "none");
    } else if (a.getAttribute("data-buy") !== "DEMO" || S.store) {
      a.href = S.store;
      a.target = "_blank"; a.rel = "noopener";
    }
  });
  if (any) {   // Payhip's overlay checkout; if it can't load, the links still go straight to Payhip
    var s = document.createElement("script");
    s.src = "https://payhip.com/payhip.js";
    document.head.appendChild(s);
  }
})();
