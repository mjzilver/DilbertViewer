def extract_metadata(metadata_div):
    tags = []
    tags_p = metadata_div.find("p", class_="small comic-tags")
    if tags_p:
        for a in tags_p.find_all("a"):
            tag_text = a.get_text(strip=True)
            if tag_text.startswith("#"):
                tag_text = tag_text[1:]
            tags.append(tag_text)
    transcript = ""
    transcript_div = metadata_div.find("div", class_="comic-transcript")
    if transcript_div:
        p = transcript_div.find("p")
        if p:
            transcript = p.get_text(strip=True)
    return transcript, tags
